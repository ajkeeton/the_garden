#pragma once
#include "sensors.h"
#include "common.h"

#define GLOBAL_MAX_SCORE 1024 // Max score for the global state
#define T_MAX_WHITE 10000 // After 10 seconds, assume the sun is out
#define T_LOW_ACTIVITY 10000 // Transition to lower activity after 10 seconds

struct meta_state_t {
    uint16_t num_sens = 1;
    pattern_state_t pulse,
        low_power,
        active;
    uint32_t t_last_trigger = 0,
             t_last_update = 0,

             // Global score is tracked two ways:
             //  1: a growing sum of the score of all triggered sensors that decays over time
             //  2: the total number of triggered sensors at a given time
             score = 0,
             count = 0;
    bool in_pulse = false;
    uint32_t t_pulse = 0;

    void init(int nsens) {
        if(!nsens)
            nsens = 1; // prevent div/0
        num_sens = nsens;

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
    }

    bool do_pulse();
    uint16_t pulse_thold();

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
        // XXX The score doesn't really tell us too much right now. Rely on count
        // XXX

        uint32_t now = millis();

        if(now - t_last_update < 100) {
            //Serial.printf("Ignoring trigger from %d, too fast\n", s.sensor);
            return;
        }

        // uint32_t val = s.value;
        t_last_update = now;

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

    void next() {
        // decay score
        if(score < 1)
            return;

        uint32_t now = millis();
        if(now - t_last_update > 100) {
            t_last_update = now;
            score = score * 99 / 100; // decaying by 1% every 100 ms (10% / second)
        }
    }

    uint16_t percent_active() {
        return count*100 / num_sens;
    }

    void log_info() {
        Serial.printf("Global state score: %lu, # trig'd: %d/%d, Pulse: %d/%d, Active: %d/%d, Low Power: %d/%d\n",
            score,
            count,
            num_sens,
            pulse.is_triggered(), pulse.score,
            active.is_triggered(), active.score,
            low_power.is_triggered(), low_power.score);
    }
};