#pragma once

#include <string>
#include "strips.h"
#include "sensors.h"
#include "common.h"
#include "state.h"
#include "common/wifi.h"

// Milliseconds to wait before sending sensor updates
// #define T_REMOTE_SENSOR_UPDATE_DELAY 100

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
    // Throttles how often we send sensor updates
    uint32_t t_last_sensor_update_sent = 0;
    // Global state
    meta_state_t state;
    // The LED strips
    strip_t strips[MAX_STRIPS];
    int nstrips = 0;
    bool wadsworth_is_full = false;

    benchmark_t bench_led_push, 
                bench_led_calcs,
                bench_sensors,
                bench_wifi;

    void log_info();

    void next_core_0() {
        // XXX If sensors.next() is here, add locking for on_trigger
        //sensors.mux.next();
    }
    
    void next_core_1();

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
        Serial.printf("wadsworth: trigger start: strip %d, pct: %u, duration: %u\n", strip, s.percent(), s.age());
        strips[strip].on_trigger(led, s.percent(), s.age());
    }

    void on_sens_cont(int strip, int led, const sensor_state_t &s) {
        // XXX Pass/use gradient?
        strips[strip].on_trigger_cont(led, s.percent(), s.age());
    }

    void on_sens_off(int strip, int led, const sensor_state_t &s) {
        state.on_trigger_off(); // add score to globlal score
        strips[strip].on_trigger_off(led, s.percent(), s.age());
    }

    void on_pir(int pir_idx) {
        if(!state.on_pir(pir_idx))
            return;

        //for(int i=0; i<nstrips; i++) {
        //    strips[i].on_pir();
        // }

        wifi.send_pir_triggered(pir_idx);
    }

    void init(bool full_wadsworth=false);
    void init_full();
    void init_deco();

    void strips_next();
};

void on_sens_trigger_start(int strip, int led, const sensor_state_t &s);
void on_sens_trigger_cont(int strip, int led, const sensor_state_t &s);
void on_sens_trigger_off(int strip, int led, const sensor_state_t &s);
void on_pir(int pir_idx);