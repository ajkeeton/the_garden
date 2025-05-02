#pragma once
#include "sensors.h"
#include "common.h"

#define GLOBAL_MAX_SCORE 1024 // Max score for the global state
#define T_MAX_WHITE 30000 // After 30 seconds, assume the sun is out
#define T_LOW_ACTIVITY 10000 // Transition to lower activity after 10 seconds
#define T_PIR_TIMEOUT 20000 // Throttle PIR to 20 seconds

enum ENERGY_STATE_t {
    STATE_SLEEP,         // Full sun, go to sleep 
    STATE_WAITING,       // Waiting for peeps
    STATE_LOW_ACTIVITY,  // Someone is possibly around
    STATE_ACTIVE,        // Some recent interaction 
    STATE_HIGH_ACTIVITY, // Active interaction at multiple sensors
    STATE_CLIMAX,        // Peak activity
};

struct pattern_state_t {
    uint32_t t_activated = 0,
            max_duration = 100, // milliseconds this change should last
            rest = 150,
            rest_until = 0; //how long to wait before we can trigger this state again
    uint8_t blend = 50; // amount to blend LEDs
    bool active = false,
        do_once = false,
        need_retrigger = false;

    uint32_t score = 0,
        thold_act = 5,
        thold_deact = 2,
        max_score = 0;

    uint32_t avg = 0;
    uint16_t n_samples = 1;

    void update_avg();
    bool can_trigger();
    bool is_triggered();
    void trigger();
    void untrigger();
    void deactivate();
    bool resting();
};

// Implements something like an ASDR for LED/kinetic behavior
struct behavior_t {
    uint32_t t_start = 0,
             // How much time to take to reach 100%
             t_attack = 0,
             // How long to hold at 100%
             t_sustain = 0,
             // How long to decay from 100% to 0%
             t_decay = 0,
             t_last = 0,
             t_delay = 0;

    uint16_t score = 0; // a percent... times 10
    bool active = false;

    void init() {
        #if 0
    white.rest = 100;
    white.max_duration = 500;
    white.blend = 10;

    pulse.rest = 500;
    pulse.max_duration = 500;
    pulse.blend = 10;

    low_power.rest = 5000;
    low_power.delay = 5000; // should be way higher

    active.max_duration = 10000; // should prob be way longer
    active.rest = 5000;
#endif

#if 0
        pulse.thold_act = 50;
        pulse.rest = 1000;
        pulse.max_duration = 500;
        pulse.max_score = 200;
        pulse.thold_deact = 10;

        active.thold_act = 5;
        active.thold_deact = 2;
        active.max_score = 200;
        
        //white.thold_deact = 3;
        low_power.thold_act = 200;
        low_power.thold_deact = 50;
        low_power.max_score = 400;
#endif
    }

    void trigger() {
        t_start = millis();
    }

    // Percent (with an extra 0) to scale a given behavior
    uint16_t amount() { 
        if(!t_start)
            return 0;

        uint32_t now = millis();
        uint32_t d = now - t_start;
        if(d < t_attack) {
            // ramp up
            score = 1000 * d / t_attack;
            //Serial.printf("in attack. score: %u\n", score);
        }
        else if(d < t_attack + t_sustain) {
            // hold
            score = 1000;
            //Serial.printf("in sustain. score: %u\n", score);
        }
        else if(d < t_attack + t_sustain + t_decay) {
            // ramp down
            score = 1000 - (1000 * (d - t_attack - t_sustain) / t_decay);
            //Serial.printf("in decay. score: %u\n", score);
        }
        else {
            score = 0;
            t_start = 0;
        }

        return score; 
    }

    bool is_triggered() {
        return score > 0;
    }
};

struct meta_state_t {
    uint16_t num_sens = 1;

    ENERGY_STATE_t state = STATE_WAITING;

    behavior_t ohai, // We were largely inactive, then a PIR gets triggered
               pending_transition, // We're about to transition states
               pulse_white, // Pulse white
               // low_power,
               sleep; // Too much light. Must be daytime.

    uint32_t t_last_trigger = 0,
             t_last_pir_update = 0,
             t_last_update = 0,

             // Global score is tracked two ways:
             //  1: a growing sum of the score of all triggered sensors that decays over time
             //  2: the total number of triggered sensors at a given time
             score = 0,
             count = 0,
             garden_score = 0,
             garden_idx = 0;

    bool in_pulse = false;
    uint32_t t_pulse = 0;

    void init(int nsens) {
        if(!nsens)
            nsens = 1; // prevent div/0
        num_sens = nsens;

        ohai.t_attack = 250;
        ohai.t_sustain = 500;
        ohai.t_decay = 1500;
    }

    // bool do_pulse();
    // uint16_t pulse_thold();

    void on_trigger_start(const sensor_state_t &s) {
        count++;
        t_last_trigger = millis();
        //Serial.printf("State: on start: %lu, %.3fs\n", s.value, (float)(s.age())/1000);
    }

    // Called whenever a sensor has stayed in a triggered state
    // The 'score' itself should range from 0 -> 1024 and reflect overall 
    // excitement
    void on_still_triggered(const sensor_state_t &s) {
        // XXX
        // XXX The score doesn't really tell us too much right now. Rely on count instead
        // XXX

        uint32_t now = millis();

        if(now - t_last_update < 100) {
            //Serial.printf("Ignoring trigger from %d, too fast\n", s.sensor);
            return;
        }

        t_last_update = now;

        // Not actually true, but set so we don't trigger on a PIR when someone leaves
        t_last_pir_update = now;

        // accumulate score from all sensors
        // score += s.value / num_sens; 
        //score += s.value / (TRIG_SPREAD_MIN-1);

        // count/num_sens for the fraction of triggers that are active
        score += s.percent() / num_sens * count/num_sens;

        if(score > GLOBAL_MAX_SCORE)
            score = GLOBAL_MAX_SCORE;
    }

    void on_trigger_off() {
        if(count == 0) {
            Serial.printf("Sensor count underflow! This should never happen!\n");
            return;
        }

        count--;
    }

    // Returns true if we should signal the gardener
    bool on_pir(int pir_idx) {
        uint32_t now = millis();
        t_last_trigger = now;
        
        // Currently only care when we're waiting
        if(state != STATE_WAITING)
            return false;

        // PIR throttling
        if(now - t_last_pir_update < T_PIR_TIMEOUT) {
            return false;
        }

        t_last_pir_update = now;

        Serial.printf("State: PIR on pin %d triggered\n", pir_idx);

        ohai.trigger();

        state = STATE_LOW_ACTIVITY;
        return true;
    }

    void next() {
        uint32_t now = millis();

        if(score < 1) {
            if(now - t_last_trigger > T_LOW_ACTIVITY) {
                // If we haven't been triggered in a while, go to sleep
                if(state != STATE_WAITING) {
                    Serial.println("State: waiting for peeps");
                    state = STATE_WAITING;
                }
            }
            return;
        }

        // decay score
        if(now - t_last_update > 100) {
            t_last_update = now;
            score = score * 99 / 100; // decaying by 1% every 100 ms (10% / second)
        }
    }

    uint16_t percent_active() {
        return count*100 / num_sens;
    }

    void handle_remote_update(uint32_t idx, uint32_t score) {
        Serial.println("Received updated garden state");
        garden_score = score;
        garden_idx = idx;
    }

    void log_info() {
        char statestr[32];
        switch(state) {
            case STATE_SLEEP:
                strcpy(statestr, "sleep");
                break;
            case STATE_WAITING:
                strcpy(statestr, "waiting");
                break;
            case STATE_LOW_ACTIVITY:
                strcpy(statestr, "low activity");
                break;
            case STATE_ACTIVE:
                strcpy(statestr, "active");
                break;
            case STATE_HIGH_ACTIVITY:
                strcpy(statestr, "high activity");
                break;
            case STATE_CLIMAX:
                strcpy(statestr, "climax");
                break;
        }

        bool pthrot = (millis() - t_last_pir_update < T_PIR_TIMEOUT);
        
        Serial.printf("State: %s, score: %lu, garden state/index: %lu/%lu sens trig'd: %d/%d, Ohai: %d/%d/%d, Pending: %d/%d, White: %d/%d, Sleep: %d/%d\n",
            statestr,
            score,
            garden_score,
            garden_idx,
            count,
            num_sens,
            ohai.is_triggered(), ohai.score, pthrot,
            pending_transition.is_triggered(), pending_transition.score,
            pulse_white.is_triggered(), pulse_white.score,
            sleep.is_triggered(), sleep.score
        );
    }
};