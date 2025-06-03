#pragma once
#include "common.h"

// It would be nice if this were over time, not total samples, but keeping it 
// simple for now
#define N_MOVING_AVG 10
#define MIN_STD_DEV_FOR_TRIGGER 75 // Used to determine threshold
#define T_CALC_STD 1000
#define MIN_THOLD 80 // Needed for noisy floating pins (even though there's a pulldown)

struct min_max_range_t {
    uint32_t 
        avg = 0,
        avg_min = INIT_TRIG_THOLD, 
        avg_max = MAX_MAX_AVG;

    float window[N_MOVING_AVG];
    uint32_t t_last_update_window = 0;

    int widx = 0;
    float
        pseudo_avg = 0,
        std_dev = 0,
        std_dev_min = 0,
        std_dev_max = 0,
        gradient = 0;

    uint32_t last_decay_min = 0,
             last_decay_max = 0;
    log_throttle_t log;
    
    min_max_range_t() {
        last_decay_min = last_decay_max = millis();
        log.log_timeout = 250;
        for(int i=0; i<N_MOVING_AVG; i++)
            window[i] = 0;
        t_last_update_window = millis();
    }
    
    void update(uint16_t val);
    void update_window(uint16_t val);

    uint32_t get_min() const;
    uint32_t get_max() const;
    uint32_t get_thold() const;
    // Slowly close the min/max range
    void decay(); 
    void log_info();
};
