#pragma once

#include <string>
#include "strips.h"
#include "sensors.h"
#include "common.h"
#include "state.h"
#include "common/wifi.h"

extern mux_t mux;
extern wifi_t wifi;

struct benchmark_t {
    uint32_t tstart = 0;
    uint32_t avg = 0;

    void start() {
        tstart = millis();
    }
    uint32_t elapsed() {
        return millis() - tstart;
    }
    void end() {
        avg *= 4;
        avg += elapsed();
        avg /= 5;
    }
};

extern benchmark_t bloop0;

class wad_t {
public:
    // Reads the mux and keeps track of sensor state
    sensors_t sensors;
    // Global state
    meta_state_t state;
    // The LED strips
    strip_t strips[MAX_STRIPS];
    int nstrips = 0;

    benchmark_t bench_led_push, 
                bench_led_calcs,
                bench_sensors,
                bench_wifi;

    void log_info() {
        EVERY_N_MILLISECONDS(1000) {
            //if(log_flags & LOG_STATE)
            //state.log_info();
            //if(log_flags & LOG_STRIPS)
            //strips[0].log_info();
            sensors.log_info();
            sensors.mux.log_info();
            Serial.printf("LED update: %lums. LED calcs: %lums. Sensors: %lums. Loop 0 (WiFi etc): %lums. WiFi read: %lums\n", 
                bench_led_push.avg, bench_led_calcs.avg, bench_sensors.avg, bloop0.avg, bench_wifi.avg);
            //for(int i=0; i<nstrips; i++) strips[i].log_info();
        }
    }
    void next_core_0() {
        log_info();
        // XXX If sensors.next() is here, add locking for on_trigger
        //sensors.mux.next();
    }
    
    void next_core_1() {
        // Check for any commands from the garden server
        bench_wifi.start();
        recv_msg_t msg;
        if(wifi.recv_pop(msg)) {
            switch(msg.type) {
                case PROTO_PULSE:
                    Serial.printf("Wadsworth handling pulse message! %lu, %u, %u, %lu \n",
                        msg.pulse.color, msg.pulse.fade, msg.pulse.spread, msg.pulse.delay);
                    //msg_pulse_t pulse(msg.get_payload(), msg.get_length());
                    for(int i=0; i<nstrips; i++) {
                        strips[i].handle_remote_pulse(
                            msg.pulse.color, 
                            msg.pulse.fade, 
                            msg.pulse.spread, 
                            msg.pulse.delay);
                    }

                    break;
                case PROTO_STATE_UPDATE:
                    // state.update(msg.get_payload(), msg.get_length());
                    break;
                default:
                    Serial.printf("Wadsworth ignoring unknown message type: 0x%X\n", msg.type);
                    break;
            }
        }
        bench_wifi.end();

        state.next(); 

        // XXX If this moves cores, don't forget the mutex for on_trigger
        bench_sensors.start();
        sensors.next();
        bench_sensors.end();

        bench_led_calcs.start();

        // XXX If wifi can be made non-blocking, move this to other core
        for(int i=0; i<nstrips; i++) {   
            if(state.low_power.is_triggered()) {
                strips[i].handle_low_power();
            }
            if(state.active.is_triggered()) {
                strips[i].handle_low_activity();
            }
            //if(state...) {
            //    strips[i].handle_low_activity();
            //}

            strips[i].background_update(state.percent_active());
        }
        bench_led_calcs.end();

        bench_led_push.start();
        strips_next();
        bench_led_push.end();
    }

    uint16_t num_strips() {
        return sizeof(strips)/sizeof(strips[0]);
    }

    void button_pressed() {
        sensors.button_pressed();
    }

    void button_hold() {
        sensors.button_hold();
    }

    void on_sens_start(int strip, int led, const sensor_state_t &s) {
        state.on_trigger_start(s); // add score to globlal score
        Serial.printf("wadsworth: pct: %u, duration: %u\n", s.percent(), s.age());
        strips[strip].on_trigger(led, s.percent(), s.age());
        wifi.send_sensor_msg(strip, led, s.percent(), s.age());
    }

    void on_sens_cont(int strip, int led, const sensor_state_t &s) {
        // XXX Pass/use gradient!
        //state.on_still_triggered(s); // add score to globlal score
        strips[strip].on_trigger_cont(led, s.percent(), s.age());
    }

    void on_sens_off(int strip, int led, const sensor_state_t &s) {
        state.on_trigger_off(); // add score to globlal score
        strips[strip].on_trigger_off(led, s.percent(), s.age());
    }

    void init();
    void strips_next();
};

void on_sens_trigger_start(int strip, int led, const sensor_state_t &s);
void on_sens_trigger_cont(int strip, int led, const sensor_state_t &s);
void on_sens_trigger_off(int strip, int led, const sensor_state_t &s);