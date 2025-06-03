
#include <Arduino.h>
#include "actuator.h"
#include "common/mux.h"
#include "common/wifi.h"

extern mux_t mux; 
extern wifi_t wifi;
const int MUX_PIN_SENS = 2; // 0 and 1 are for pot and limit
const int SENS_THOLD = 200;

void actuator_t::init(const char *n, int pot, int limit, int pwm, int dir1, int dir2)
{
    name = n;
    pin_pot = pot;
    pin_limit = limit;
    pin_pwm = pwm;
    pin_dir1 = dir1;
    pin_dir2 = dir2;
    pinMode(pin_pwm, OUTPUT);
    pinMode(pin_dir1, OUTPUT);
    pinMode(pin_dir2, OUTPUT);
    stop();
}

bool actuator_t::near_target_start() {
    int16_t pot = mux.read_raw(pin_pot);

    if(abs(pot_target - pot) > 35) 
        return false;

    stop();
    return true;
}

bool actuator_t::near_target_stop() {
    int16_t pot = mux.read_raw(pin_pot);

    if(abs(pot_target - pot) > 5) 
        return false;

    stop();
    return true;
}

void actuator_t::do_test_cycle() {
    if(near_target_stop()) {
        t_delay_until = millis() + 1000; // 1 second delay
        // Change direction
        pot_target = (pot_target == pot_at_end) ? pot_at_start : pot_at_end;
        return;
    }

    int16_t pot = mux.read_raw(pin_pot);
    
    if(pot_target > pot) {
        // Move up
        //Serial.printf("Moving up: Pot %d, Target %d\n", pot, pot_target);
        set_direction_up(true);
    } 
    else {
        // Move down
        //Serial.printf("Moving down: Pot %d, Target %d\n", pot, pot_target);
        set_direction_up(false);
    } 
    
    //Serial.printf("HERE: speed: %d, pot: %d, target: %d\n", speed, pot, pot_target);
    analogWrite(pin_pwm, speed);
}

void actuator_t::init_test_cycle() {
    if(mux.read_raw(pin_pot) >= pot_at_end*.9) {
        // If the pot is already above the target, set to bottom
        // This should not have happened
        pot_target = pot_at_start;
        dir_up = false; 
    } 
    else { 
        pot_target = pot_at_end;
        dir_up = true;
    }
    
    state = STATE_TEST_CYCLE;
}

void actuator_t::init_wakeup() {
    state = STATE_WAKEUP;
    state_next = STATE_REST;
    pot_target = POT_TARGET_WAKE;
    
    Serial.println("Waking up actuator...");
    if(near_target_start()) {
        // Move... down a little?
        pot_target *= .8;
        t_delay_until = millis() + 100; 
    }
}
void actuator_t::init_next() {
    state = state_next;
    t_delay_until = millis() + 100; // brief delay makes ensures mux can be caught up

    switch(state) {
        case STATE_WAKEUP:
            init_wakeup();
            break;

        case STATE_IDLE:
            pot_target = pot_at_end * .9; // Idle at top
            break;

        case STATE_SIGH_START:
            init_sigh();
            break;

        case STATE_SIGH_END:
            init_sigh_end();
            break;

        case STATE_BREATHE_SHALLOW:
            pot_target = pot_at_end * .8; // Breathe shallow
            break;

        case STATE_BREATHE:
            pot_target = pot_at_end * .4; // Breathe deeper
            break;

        case STATE_REST:
            do_rest();
            break;

        case STATE_TEST_CYCLE:
            init_test_cycle();
            break;

        case STATE_MOVE_TO_TGT:
            do_move_to_target();
            break;

        case STATE_SLEEP:
            break;
            
        default:
            Serial.printf("Unknown state: %d\n", state);
    }
}

void actuator_t::do_move_to_target() {
    if(near_target_stop()) {
        end();
        int old_tgt = pot_target;
        init_next();
        return;
    }

    int16_t pot = mux.read_raw(pin_pot);

    if(pot_target > pot) {
        set_direction_up(true);
        digitalWrite(pin_pwm, HIGH);
    } 
    else { 
        set_direction_up(false);
        digitalWrite(pin_pwm, HIGH);
    }

    speed = DEF_SPEED;
    analogWrite(pin_pwm, speed);
}

void actuator_t::do_rest() {
    pot_target = pot_at_end * .9; // We rest at top
    if(near_target_start()) {
        pot_target = mux.read_raw(pin_pot);
        stop();
        state = STATE_IDLE;
        return;
    }
    do_move_to_target();
}

void actuator_t::init_sigh() {
    pot_target = pot_at_end * .95; // Start sigh at 60% of max
    if(!near_target_start()) {
        state = STATE_SIGH_START;
        state_next = STATE_SIGH_END;
    } 
    else {
        t_delay_until = millis() + 1000; // 1 second delay before next action
        state = STATE_IDLE;
        stop();
    }
}

void actuator_t::init_sigh_end() {
    pot_target = pot_at_end*.7;
    state = STATE_SIGH_END;
    state_next = STATE_WAKEUP;

    if(!near_target_start()) {
        Serial.printf("Sigh end starting: %d vs %d\n", 
            mux.read_raw(pin_pot), pot_target);
    }
    else {
        Serial.printf("Sigh end already at target: %d vs %d\n", 
            mux.read_raw(pin_pot), pot_target);
        state = STATE_IDLE;
        stop();  
    }
}

void actuator_t::do_idle() {
    uint32_t now = millis();
    uint32_t since_last = now - t_last_move;

    // Do a sigh. SooOoOo bored
    if(since_last > 5000) { // 5 seconds idle
        init_sigh();
        return;
    }

    #if 0
    // If we're in pattern 1, do some breathing
    if(pattern_idx == 1) {
        // Breathe shallow
        if(since_last > 2000) { // 2 seconds
            state = STATE_BREATHE_SHALLOW;
            pot_target = pot_at_end * .8; // Breathe shallow
            do_move_to_target();
        } else if(since_last > 1000) { // 1 second
            state = STATE_BREATHE;
            pot_target = pot_at_end * .7; // Breathe deeper
            do_move_to_target();
        }
    } 
    else if(pattern_idx == 2) {
        // Normal breathing
        if(since_last > 1000) { 
            state = STATE_BREATHE_SHALLOW;
            pot_target = pot_at_end * .8; // Breathe shallow
            do_move_to_target();
        } else if(since_last > 1500) { // 1.5 seconds
            state = STATE_BREATHE;
            pot_target = pot_at_end * .7; // Breathe deeper
            do_move_to_target();
        }
    } 
    else {
        // Default to resting
        state = STATE_REST;
    }
    #endif
}

void actuator_t::step() {
    if((mux.read_raw(pin_limit) > 512) && dir_up) {
        stop();
        int e = mux.read_raw(pin_pot);
        Serial.printf("Actuator reached limit @ %d, stopping.", e);
        state = STATE_REST;
        pot_at_end = e;
    } else {
        uint16_t sens = mux.read_raw(MUX_PIN_SENS);
        if(sens > SENS_THOLD) {
            handle_sensor(sens);
            wifi.send_sensor_msg(0, 0, 100, 0);
        }
    }

    uint32_t now = millis();
    if(now < t_delay_until) {
        // If we're in a delay, just return
        return;
    } 

    if(!near_target_stop() && speed) {
        t_last_move = now;
    }

    switch(state) {
        case STATE_WAKEUP:
            do_move_to_target();
            break;

        case STATE_IDLE:
            do_idle();
            break;

        case STATE_BREATHE_SHALLOW:
        case STATE_BREATHE:
        case STATE_SIGH_START:
        case STATE_SIGH_END:
            do_move_to_target();
            break;

        case STATE_REST:
            do_rest();
            break;

        case STATE_TEST_CYCLE:
            do_test_cycle();
            break;

        case STATE_MOVE_TO_TGT:
            do_move_to_target();
            break;

        case STATE_SLEEP:
            do_move_to_target();
            break;
        default:
            Serial.printf("Unknown state: %d\n", state);
            state = STATE_IDLE;
            break;
    }
}

void actuator_t::set_direction_up(bool up, int s) {
    // when changing directions, always stop and delay
    if(up != dir_up) {
        stop();
        delay(50); // Allow time for the actuator to stop
    }

    dir_up = up;
    speed = s;

    if (!up) {
        digitalWrite(pin_dir1, HIGH);
        digitalWrite(pin_dir2, LOW);
    } else {
        digitalWrite(pin_dir1, LOW);
        digitalWrite(pin_dir2, HIGH);
    }

    analogWrite(pin_pwm, speed);
}

void actuator_t::set_direction_up(bool up) {
    speed = DEF_SPEED;
    set_direction_up(up, speed);
}

void actuator_t::stop() {
    speed = 0;
    analogWrite(pin_pwm, speed);
    digitalWrite(pin_dir1, LOW);
    digitalWrite(pin_dir2, LOW);
    t_last_stop = millis();
}

void actuator_t::end() {
    stop();
    pot_target = mux.read_raw(pin_pot);
}

void actuator_t::handle_button() {
    int16_t pot = mux.read_raw(pin_pot);
    if(pot < midpoint())
        pot_target = pot_at_end;
    else
        pot_target = pot_at_start;
    state = STATE_MOVE_TO_TGT;
}

void actuator_t::handle_sensor(uint16_t sens) {
    pot_target = POT_TARGET_WAKE;
    state = STATE_MOVE_TO_TGT;
}

void actuator_t::sleepy_time() {
    // Move actuators to rest and 
    pot_target = DEF_POT_AT_MAX;
    state = STATE_SLEEP;
}

void actuator_t::log_periodic() {
    unsigned long now = millis();
    if (now - t_last_log < 50) {
        return;
    }
    t_last_log = now;
    
    const char* state_str = "";
    switch(state) {
        case STATE_WAKEUP:         state_str = "wakeup"; break;
        case STATE_IDLE:           state_str = "idle"; break;
        case STATE_SIGH_START:     state_str = "sigh_start"; break;
        case STATE_SIGH_END:       state_str = "sigh_end"; break;
        case STATE_BREATHE_SHALLOW:state_str = "breathe_shallow"; break;
        case STATE_BREATHE:        state_str = "breathe"; break;
        case STATE_REST:           state_str = "rest"; break;
        case STATE_TEST_CYCLE:     state_str = "test_cycle"; break;
        case STATE_MOVE_TO_TGT:    state_str = "move_to_target"; break;
        case STATE_SLEEP:          state_str = "sleep"; break;
        default:                   state_str = "unknown"; break;
    }
    
    Serial.printf("%s:\n\tState: %s, Tgt: %d, Up: %d, Pot: %d, Limit: %d, Sens: %d, Since last: %.3fs\n", 
            name, state_str, pot_target, dir_up, 
            mux.read_raw(pin_pot), 
            mux.read_raw(pin_limit),
            mux.read_raw(MUX_PIN_SENS),
            (float)(now - t_last_move) / 1000.0);
}