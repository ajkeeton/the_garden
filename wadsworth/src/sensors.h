#pragma once

#include "common.h"

struct min_max_range_t {
    //uint16_t samples = 0;
    uint32_t avg_min = INIT_TRIG_THOLD, 
                avg_max = INIT_TRIG_MAX,
                std_dev_min = 0;
    uint32_t last_decay_min = 0,
             last_decay_max = 0;
    log_throttle_t log;
    uint8_t pin = 255;
    
    min_max_range_t() {
        last_decay_min = last_decay_max = millis();
        log.log_timeout = 250;
    }
    
    void update(uint16_t val);
    uint32_t get_min() const;
    uint32_t get_max() const;
    uint32_t get_thold() const;
    void decay(); // slowly close the range
    void debug();
};
  
// NOTE: A score is given as a magnitude and duration
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

        if(now - t_triggered_last_update > 25) {
           // Every 25 ms decay the score by 10%
            value = 0.9 * value;
            t_triggered_last_update = now;
            //Serial.printf("%lu\n", score);
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
        value = val; // XXX use short moving average?
        t_triggered_last_update = now;

        if(!t_trigger_start) {
            t_trigger_start = now;
        }
        
        return true;
    }

    uint16_t percent() const {
        if(value < minmax.get_thold())
            return 0;
        // XXX This will return >100% if the value is above the max average
        return map(value, minmax.get_thold(), minmax.get_max(), 0, 100);
    }

    uint32_t age() const {
        if(t_trigger_start == 0)
            return 0;
        return millis() - t_trigger_start;
    }
};

struct sensors_t {
    sensor_state_t sensors[MAX_MUX_IN];
    mux_t mux;
    int sens_start = 0,
        sens_end = 0;

    void (*on_trigger_start)(int, int, const sensor_state_t &) = NULL;
    void (*on_is_triggered)(int, int, const sensor_state_t &) = NULL;
    void (*on_trigger_off)(int, int, const sensor_state_t &) = NULL;

    // Mapping of sensor to strip and LED index
    // To keep it simple, just using an array it uints, where first 2 bytes is the strip, next 2 is the LED
    uint32_t map_strip_led[MAX_MUX_IN];

    sensors_t();
    void init(int pin_start, int pin_end, 
            void (*start)(int, int, const sensor_state_t &),
            void (*is)(int, int, const sensor_state_t &),
            void (*off)(int, int, const sensor_state_t &));
    void add(uint16_t sensor, uint16_t strip, uint16_t led);

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