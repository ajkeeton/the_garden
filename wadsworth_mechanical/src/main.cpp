#include <Arduino.h>
#include <FastLED.h>
#include "actuator.h"
#include "common/common.h"
#include "common/mux.h"
#include "common/wifi.h"

#define IN_BUTTON A2
#define IN_DIP A1

const int PIN_PWM1 = 1;
const int PIN_DIR1 = 2;
const int PIN_DIR2 = 3;

const int PIN_PWM2 = 4;
const int PIN_DIR3 = 5;
const int PIN_DIR4 = 6;

// Setup global for actuators, initialized in setup()
actuator_t primary, secondary;
mux_t mux;
wifi_t wifi;

void wait_for_serial() {
    // Wait a couple seconds for  Serial to be ready
    while (!Serial && millis() < 2000) {
        delay(50);
    }
}

void setup() {
    Serial.begin(9600);

    pinMode(LED_BUILTIN, OUTPUT);
    blink();

    wifi.init();
    wait_for_serial();
}

void setup1()
{    
    mux.init();
    mux.n_avg = 5;
    pinMode(IN_BUTTON, INPUT_PULLUP);
    pinMode(IN_DIP, INPUT_PULLUP);

    // Initialize hardware
    analogWriteFreq(1024);

    primary.init("Primary", DEF_MUX_PIN_POT_1, DEF_MUX_PIN_LIMIT_1, PIN_PWM1, PIN_DIR1, PIN_DIR2);
    secondary.init("Secondary", DEF_MUX_PIN_POT_2, DEF_MUX_PIN_LIMIT_2, PIN_PWM2, PIN_DIR3, PIN_DIR4);

    wait_for_serial();

    if(analogRead(IN_DIP) < 512) {
        Serial.println("DIP switch is ON, doing test cycle");

        primary.init_test_cycle(); 
        secondary.init_test_cycle();
    }
    else {
        primary.init_wakeup();
        secondary.init_wakeup();
    }

    Serial.println("Starting...");
}

void loop() {
    wifi.next();
}

// XXX 
// There must have been a compiler bug, having these as statics inside
// is_button_pressed() caused a mutex to not release in the wifi code
bool last = HIGH;
bool logged_once = false;
uint32_t t_last_press = 0;

bool is_button_pressed() {
    bool current = analogRead(IN_BUTTON) < 512 ? LOW : HIGH;
 
    uint32_t now = millis();

    if (current != last) {
        t_last_press = now;
    }

    last = current;

    if ((now - t_last_press) > 30 && current == LOW) {
        if(!logged_once) {
            logged_once = true;
            return true;
        }

        return false;
    }

    logged_once = false;
    return false;
}

void loop1() {
    static int last_state = 0;

    // Blink the built-in LED to indicate activity
    blink();
    mux.next();

    if(is_button_pressed()) {
        primary.handle_button();
        secondary.handle_button();
        Serial.println("Button pressed");
    }

    recv_msg_t msg;
    int loop_counter = 0;
    while(wifi.recv_pop(msg)) {
        switch(msg.type) {
            case PROTO_PULSE:
                Serial.printf("The gardener told us to pulse the LEDs: %u %u %u %u\n", 
                    msg.pulse.color, msg.pulse.fade, msg.pulse.spread, msg.pulse.delay);
                break;
            case PROTO_STATE_UPDATE:
                Serial.printf("A state update message: %u %u\n", 
                        msg.state.pattern_idx, msg.state.score);
                last_state = msg.state.pattern_idx;
                break;
            case PROTO_PIR_TRIGGERED:
                Serial.println("A PIR sensor was triggered");
                break;
            default:
                Serial.printf("Unknown message type: %u\n", msg.type);
        }
    }

    //primary.step();
    secondary.step();
    
    primary.log_periodic();
    secondary.log_periodic();
}

#if 0
#define STOP_MAIN_PIN A2
#define STOP_SECONDARY_PIN A1

Actuator* actuator_main = nullptr;
Actuator* actuator_secondary = nullptr;

// Movement settings
const unsigned long pause_duration = 150; // Pause between direction changes

// State variables for the cycling logic
enum CycleState { START_UP, MOVING_UP, PAUSE_BEFORE_DOWN, START_DOWN, MOVING_DOWN, PAUSE_BEFORE_UP };

CycleState main_cycle_state = START_UP;
CycleState secondary_cycle_state = START_UP;
unsigned long main_state_change_time = 0;
unsigned long secondary_state_change_time = 0;

// Forward declarations for callbacks
void on_main_complete(Actuator* actuator);
void on_secondary_complete(Actuator* actuator);
void on_main_limit(Actuator* actuator);
void on_secondary_limit(Actuator* actuator);

void setup()
{
        Serial.begin(9600);

    // Initialize hardware
    analogWriteFreq(1024);

    pinMode(PWM1, OUTPUT);
    pinMode(PWM2, OUTPUT);
    pinMode(DIR1, OUTPUT);
    pinMode(DIR2, OUTPUT);
    pinMode(DIR3, OUTPUT);
    pinMode(DIR4, OUTPUT);
    pinMode(STOP_MAIN_PIN, INPUT);
    pinMode(STOP_SECONDARY_PIN, INPUT);

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(DIR1, 0);
    digitalWrite(DIR2, 1);
    digitalWrite(DIR3, 0);
    digitalWrite(DIR4, 1);

    delay(100);

    // Initialize Actuators
    actuator_main = new Actuator("main", PWM1, DIR1, DIR2, STOP_MAIN_PIN);
    actuator_secondary = new Actuator("secondary", PWM2, DIR3, DIR4, STOP_SECONDARY_PIN);

    // Set callbacks for movement completion and limit reached
    actuator_main->on_movement_complete = on_main_complete;
    actuator_main->on_limit_reached = on_main_limit;
    actuator_secondary->on_movement_complete = on_secondary_complete;
    actuator_secondary->on_limit_reached = on_secondary_limit;

    // Reset actuators to starting bottom position
    actuator_main->move_down(150);
    actuator_secondary->move_down(150);
    delay(5000);
    actuator_main->stop();
    actuator_secondary->stop();

    Serial.println("Starting...");

    // Initialize the cycle state machine
    main_cycle_state = START_UP;
    secondary_cycle_state = START_UP;
    main_state_change_time = millis();
    secondary_state_change_time = millis();
}


/** Callback functions **/
void on_main_complete(Actuator* actuator)
{
    Serial.println("Main actuator movement completed");
    // Any specific actions when main actuator completes movement
}

void on_secondary_complete(Actuator* actuator)
{
    Serial.println("Secondary actuator movement completed");
    // Any specific actions when secondary actuator completes movement
}

void on_main_limit(Actuator* actuator)
{
    Serial.println("Main actuator reached limit");
    // Reset the actuator by moving it down for 7000ms at speed 300
    actuator->reset(300, 7000);
}

void on_secondary_limit(Actuator* actuator)
{
    Serial.println("Secondary actuator reached limit");
    // Reset the actuator by moving it down for 7000ms at speed 300
    actuator->reset(300, 7000);
}
/** End Callback Functions */

// Example cycle pattern with state handling
void cycle(int speed, int duration, Actuator* actuator, CycleState& cycle_state, unsigned long& state_change_time)
{
    unsigned long current_time = millis();
    bool idle = actuator->is_idle();

    switch (cycle_state) {
    case START_UP:
        actuator->start_moving_up(speed, duration);
        cycle_state = MOVING_UP;
        state_change_time = current_time;
        break;

    case MOVING_UP:
        // Wait for actuator to complete its movement
        if (idle) {
            cycle_state = PAUSE_BEFORE_DOWN;
            state_change_time = current_time;
        }
        break;

    case PAUSE_BEFORE_DOWN:
        // Small pause before changing direction
        if (current_time - state_change_time >= pause_duration) {
            cycle_state = START_DOWN;
            state_change_time = current_time;
        }
        break;

    case START_DOWN:
        actuator->start_moving_down(speed, duration);
        cycle_state = MOVING_DOWN;
        state_change_time = current_time;
        break;

    case MOVING_DOWN:
        // Wait for actuator to complete its movement
        if (idle) {
            cycle_state = PAUSE_BEFORE_UP;
            state_change_time = current_time;
        }
        break;

    case PAUSE_BEFORE_UP:
        // Small pause before starting the cycle again
        if (current_time - state_change_time >= pause_duration) {
            cycle_state = START_UP;
            state_change_time = current_time;
        }
        break;
    }
}

void loop()
{
    blink();

    // Update each actuator (checks limits, handles timing)
    actuator_main->update();
    actuator_secondary->update();

    cycle(200, 5000, actuator_main, main_cycle_state, main_state_change_time);
    cycle(200, 3000, actuator_secondary, secondary_cycle_state, secondary_state_change_time);
}
#endif