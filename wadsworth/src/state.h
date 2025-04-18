#pragma once
#include "sensors.h"
#include "common.h"

#define GLOBAL_MAX_SCORE 1000 // max score for the global state
#define T_MAX_WHITE 10000 // After 10 seconds, assume the sun is out
#define T_LOW_ACTIVITY 10000 // Transition to lower activity after 10 seconds

struct meta_state_t {
    uint16_t num_sens = 1;
    pattern_state_t pulse,
        low_power,
        active;
    uint32_t t_last_trigger = 0,
             t_last_update = 0,
             score = 0;
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

    void on_trigger(const sensor_state_t &s) {
        uint32_t now = millis();

        if(now - t_last_trigger < 100) {
            //Serial.printf("Ignoring trigger from %d, too fast\n", s.sensor);
            return;
        }

        // uint32_t val = s.value;
        t_last_trigger = now;
        // score += s.value / num_sens; // accumulate score from all sensors
        score += s.value / (TRIG_SPREAD_MIN-1);

        if(score > GLOBAL_MAX_SCORE)
            score = GLOBAL_MAX_SCORE;
    }

    void next() {
        // decay score
        if(score < 1)
            return;

        uint32_t now = millis();
        if(now - t_last_update > 100) {
            t_last_update = now;
            score = (score * 99) / 100; // decaying by 1% every 100 ms
        }
    }

    void log_info() {
        Serial.printf("Global state score: %lu, Pulse: %d/%d, Active: %d/%d, Low Power: %d/%d\n",
            score,
            pulse.is_triggered(), pulse.score,
            active.is_triggered(), active.score,
            low_power.is_triggered(), low_power.score);
    }
};