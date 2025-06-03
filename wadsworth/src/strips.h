#pragma once
#include <map>
#include "state.h"
#include "common.h"

#define N_STATE_SAMPLES 5

struct sens_glow_t {
    void update(uint16_t led, uint32_t val) {

    }
};

struct sens_pulse_t {
    uint32_t tstart = 0;

    void update(uint16_t led, uint32_t val) {
        if(val) {
            if(!tstart )
                tstart = millis();
        }
        else 
            tstart = 0;
    }
};

struct strip_t {
    uint8_t id = 0; // Strip ID, used for debugging

    // The current LED animation
    uint8_t pattern = 0;
    uint16_t num_leds = 0;
    uint32_t t_last_update = 0;
  
    CRGB *leds = NULL;

    layer_t layer_tracers,
            layer_waves,
            layer_transition,
            layer_colored_glow,
            layer_white;

    animate_pulse_white_t pulse_white;
    animate_waves_t waves;
    wave_pulse_t wave_pulse;
    
    //tracer_t tracers_sens[MAX_MUX_IN+1]; // Tracers for each sensor, plus one for the central pire
    tracer_v2_t tracer_sens; // The tracers/pulses triggered with sensors 
    tracer_v2_t reverse_pulse; // The tracers/pulses triggered remotely
    bool *triggered = NULL;

    tracer_t tracers_rand[MAX_RIPPLES_RAND];
    uint16_t n_rand_tracers = MAX_RIPPLES_RAND;

    rainbow_t rainbow;
    blobs_t blobs;
    blob_asc_t blob_asc;
    
    //climb_white_t white;

    // The center spire gets special treatment
    bool is_center = false;
    // If we use one strip with the sensor in the middle, and want the trigger
    // effect to go up and down, set this
    bool trigger_both_directions = false;

    void init(CRGB *l, uint16_t nleds, bool is_ctr=false);
    void step(uint32_t global_activity);
    CRGB trig_target_color();
    void on_trigger(uint16_t led, uint16_t score, uint32_t duration);
    void on_trigger_cont(uint16_t led, uint16_t score, uint32_t duration);
    void on_trigger_off(uint16_t led, uint16_t percent, uint32_t duration);
    void background_update(meta_state_t &state);

    void find_mids();
    bool near_mids(int i);
    void do_rainbow();
    void do_basic_ripples(uint16_t activity);
    void do_high_energy_ripples(uint16_t activity);
    void fade_all(uint16_t amount);

    // Gardener told us to start a pulse
    void handle_remote_pulse(uint32_t color, uint8_t fade, uint16_t spread, uint32_t delay);
    void handle_remote_state_change(uint16_t pat);

    // Force all LEDs white, skip the layers. Mostly for debugging
    void force_white();

    void log_info() {
        int ttracers = 0;
        //for(int i=0; i<sizeof(tracers_sens)/sizeof(tracer_t); i++)
        //    ttracers += tracers_sens[i].exist;
            
        for(int i=0; i<n_rand_tracers; i++)
            ttracers += tracers_rand[i].exist;

        Serial.printf("Strip %d, pattern %d, tracers %d\n", 
            id, pattern, ttracers);
    }
  };
  