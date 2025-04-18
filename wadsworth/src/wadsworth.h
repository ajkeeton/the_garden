#pragma once

#include <string>
#include "strips.h"
#include "sensors.h"
#include "common/mux.h"
#include "common.h"
#include "state.h"

class wad_t {
public:
    // Reads the mux and keeps track of sensor state
    sensors_t sensors;
    // Global state
    meta_state_t state;
    // The LED strips
    strip_t strips[MAX_STRIPS];
    int nstrips = 0;

    void log_info() {
        EVERY_N_MILLISECONDS(1000) {
            state.log_info();
            sensors.log_info();
            strips[0].log_info();
            //for(int i=0; i<nstrips; i++) strips[i].log_info();
        }
    }
    void next_core_0() {
        // XXX If sensors.next() is here, add locking for on_trigger
    }
    
    void next_core_1() {
        log_info();

        state.next();
        sensors.next(); // XXX If this moves core, need to add locking for on_trigger XXX

        for(int i=0; i<nstrips; i++) {
            // XXX If wifi can be made non-blocking, move this to other core
            if(state.low_power.is_triggered()) {
                strips[i].handle_low_power();
            }
            else if(state.active.is_triggered()) {
                strips[i].handle_low_activity();
            }
            else if(state.pulse.is_triggered()) {
                strips[i].handle_pulse();
            }
            else {
                strips[i].handle_low_activity();
            }

            strips[i].background_update(state.score);
        }

        next();
    }

    uint16_t num_strips() {
        return sizeof(strips)/sizeof(strips[0]);
    }

    void button_override() {
        sensors.button_override();
    }

    void on_trigger(int strip, int led, const sensor_state_t &s) {
        state.on_trigger(s);
        strips[strip].on_trigger(led, s.percent(), s.duration);
    }

    void init();
    void next();
};

void on_trigger(int strip, int led, const sensor_state_t &s);