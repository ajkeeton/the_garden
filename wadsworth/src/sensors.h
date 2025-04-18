#pragma once

#include "common.h"
#include "common/mux.h"

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
                t_triggered_last_seen = 0,
                t_triggered_last_update = 0;
    uint32_t value = 0,
                duration = 0;
    min_max_range_t minmax;

    uint32_t score_max() {
        // XXX figure out
        return 100000;
    }

    void decay() {
        uint32_t now = millis();

        // At zero, reset t_trigger_start
        if (value > 1) {
            //Serial.printf("Score decayed from %lu to: ", score);
        
            float d = now - t_triggered_last_seen;
            d /= 2500; 
        
            if(d > value)
                value = 0;
            else
                value -= d;
            //Serial.printf("%lu\n", score);
        }
        else
            t_trigger_start = 0;

        if(duration && now - t_triggered_last_update > 50) {
            t_triggered_last_update = now;
            duration--;
        }
    }

    bool update(uint16_t val) {
        // Update the minmax range
        minmax.update(val);
        //score = minmax.get_max() - minmax.get_thold()
        uint32_t now = millis();

        if(val < minmax.get_thold()) {
            decay();
            return false;
        }
    
        value = val;

        if(t_trigger_start) {
            if(duration < 500 && now - t_triggered_last_update > 250) {
                t_triggered_last_update = now;
                duration++;
            } 
        }
        else {
            t_trigger_start = now;
            duration = 1;
        }
        
        return true;
    }

    uint16_t percent() const {
        if(value < minmax.get_thold())
            return 0;
        return map(value, minmax.get_thold(), minmax.get_max(), 0, 100);
    }
};

struct sensors_t {
    sensor_state_t sensors[MAX_MUX_IN];
    mux_t mux;
    int sens_start = 0,
        sens_end = 0;
    void (*on_trigger)(int, int, const sensor_state_t &) = NULL;

    // Mapping of sensor to strip and LED index
    // To keep it simple, just using an array it uints, where first 2 bytes is the strip, next 2 is the LED
    uint32_t map_strip_led[MAX_MUX_IN];

    sensors_t() {
        mux.init();

        for(int i=0; i<MAX_MUX_IN; i++) {
            map_strip_led[i] = 0; // strip 0, LED 0
        }
    }

    void add(uint16_t sensor, uint16_t strip, uint16_t led) {
        if(sensor >= MAX_MUX_IN) {
            Serial.printf("Sensor %u is out of range, max is %d\n", sensor, MAX_MUX_IN);
            return;
        }

        map_strip_led[sensor] = ((uint32_t)(strip) << 16) | led;
    }

    void init(int pin_start, int pin_end, void (*ot)(int, int, const sensor_state_t &)) {
        sens_start = pin_start;
        sens_end = pin_end;
        on_trigger = ot;
        
        // Not all inputs on the mux are sensors
        for (int i = sens_start; i < sens_end; i++) {
            //scores[i].init(on_trigger);
        }
    }

    void do_trigger(int i) {
        int sidx = (map_strip_led[i] >> 16) & 0xFFFF; // Strip index
        int lidx = map_strip_led[i] & 0xFFFF; // LED index
        on_trigger(sidx, lidx, sensors[i]);
    }

    void next() {
        mux.next();

        for(int i=sens_start; i<sens_end; i++) {
            uint16_t raw = mux.read_raw(i);
            if(sensors[i].update(raw))
                do_trigger(i);
        }
    }

    uint16_t num_sens() {
        return sens_end - sens_start;
    }

    void button_override() {
        if(!sensors[0].update(1024)) // Pretend the first sensor is triggered
            return;
        do_trigger(0);
    }

    void log_info() {
        #if 0
        for(int i=sens_start; i<sens_end; i++) {
            if(sensors[i].value) {
                Serial.printf("Sensor %d: value: %u, duration: %u, minmax: %u/%u/%u\n", 
                    i, sensors[i].value, sensors[i].duration, 
                    sensors[i].minmax.get_min(), sensors[i].minmax.get_max(), sensors[i].minmax.get_thold());
            }
        }
        #endif
        int i=0;
        Serial.printf("Sensor %d: value: %u, duration: %u, minmax: %u/%u/%u\n", 
            i, sensors[i].value, sensors[i].duration, 
            sensors[i].minmax.get_min(), 
            sensors[i].minmax.get_max(), 
            sensors[i].minmax.get_thold());
    }
};