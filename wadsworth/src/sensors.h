#pragma once

#include "common.h"
#include "minmax.h"

// State of one specific sensor
// Handles smoothing of a sensor's value and determining what constitutes a 
// trigger
struct sensor_state_t {
    uint32_t t_trigger_start = 0,
             t_triggered_last_update = 0;
    uint32_t value = 0;
    min_max_range_t minmax;

    uint32_t score_max() {
        // XXX figure out
        return 100000;
    }

    void decay() {
        if(!value) {
            t_trigger_start = 0;
            return;
        }

        uint32_t now = millis();

        if(now - t_triggered_last_update > 15) {
           // Every 15 ms decay the score by 5%
            value = 0.95 * value;
            t_triggered_last_update = now;
            //if(t_trigger_start) Serial.printf("DECAYING -> %d\n", value);
        }
    }

    bool update(uint16_t val) {
        minmax.update(val);

        if(val < minmax.get_thold()) {
            // rather than just switching off, let trigger fade
            decay();
            return false;
        }

        uint32_t now = millis();
        //value = (value * 4 + val)/5; // XXX use short moving average? Do away with? Review
        if(val > value) value = val;
        t_triggered_last_update = now;

        //Serial.printf("val > thold. %u > %u, 'value': %u, max: %u\n", 
        //        val, minmax.get_thold(), value, minmax.get_max());

        if(!t_trigger_start) {
            t_trigger_start = now;
        }
        
        return true;
    }

    uint16_t percent() const {
        if(value <= minmax.get_thold())
            return 0;

        //Serial.printf("sensor score: %lu, %u %lu\n", 
        //        value, minmax.get_thold(), minmax.get_max());
        // NOTE: This will return >100% if the value is above the max average
        // The overflow issue from before was when get_thold > get_max
        return map(value, minmax.get_thold(), minmax.get_max(), 1, 100);
    }

    uint32_t age() const {
        if(t_trigger_start == 0)
            return 0;
        return millis() - t_trigger_start;
    }

    bool gradient_positive() const {
        // XXX TODO: use for only pulsing when the value is increasing or stable
        // Need to compute gradient over some time window
        return minmax.gradient >= 0; 
    }
};

#define MAX_STRIP_SHARED_SENS 4
struct sensor_to_led_map_t {
    uint16_t strip[MAX_STRIP_SHARED_SENS];
    uint16_t led[MAX_STRIP_SHARED_SENS];
    uint16_t sens_index = 0;
    int nshared = 0;

    // Allow up to 4 strips to share a sensor, this way they can have unique 
    // strips on multiple sides

    sensor_to_led_map_t() {
        init();
    }

    void init() {
        nshared = 0;
        for(int i=0; i<MAX_STRIP_SHARED_SENS; i++) {
            strip[i] = 0;
            led[i] = 0;
        }
    }

    void add(uint16_t sidx, uint16_t s, uint16_t l) {
        if(nshared >= MAX_STRIP_SHARED_SENS)
            return;
        sens_index = sidx;
        strip[nshared] = s;
        led[nshared] = l;
        nshared++;
    }
};

// Interface to all sensors
struct sensors_t {
    sensor_state_t sensors[MAX_MUX_IN];

    // PIR sensors
    bool pir_map[MAX_MUX_IN];

    mux_t mux;
    int sens_start = 0,
        sens_end = 0;

    // Mapping of sensor to strip and LED index
    // To keep it simple, just using an array it uints, where first 2 bytes is the strip, next 2 is the LED
    //uint32_t map_strip_led[MAX_MUX_IN];
    sensor_to_led_map_t sensor_to_led_map[MAX_MUX_IN];

    // Need to periodically tell the server about the sensor state
    uint32_t t_last_sensor_update_sent = 0;

    void (*on_trigger_start)(int, int, const sensor_state_t &) = NULL;
    void (*on_is_triggered)(int, int, const sensor_state_t &) = NULL;
    void (*on_trigger_off)(int, int, const sensor_state_t &) = NULL;
    void (*on_pir)(int) = NULL;

    sensors_t();
    void init(int pin_start, int pin_end, 
            void (*start)(int, int, const sensor_state_t &),
            void (*is)(int, int, const sensor_state_t &),
            void (*off)(int, int, const sensor_state_t &),
            void (*pir)(int)
        );
    void add(uint16_t sensor, uint16_t strip, uint16_t led);
    void add_pir(uint16_t mux_pin);
    void do_on_trigger_start(int i);
    void do_is_triggered(int i);
    void do_on_trigger_off(int i);

    void next();

    uint16_t num_sens() {
        return sens_end - sens_start;
    }

    // Pretend the first sensor is triggered
    void button_pressed();
    void button_hold();
    void log_info();
};