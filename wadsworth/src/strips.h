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

struct layer_t {
    CRGB *targets = NULL;
    // XXX switch to millis/elapsed
    uint16_t *ttl = NULL;
    bool use_ttl = false; 
    uint16_t num_leds = 0;
    uint32_t last_update = 0;

    // n: number of LEDs
    // us: use time-to-live
    void init(uint16_t n, bool us) {
        use_ttl = us;

        if(targets)
            delete []targets;
        targets = new CRGB[n];

        if(use_ttl) {
            if(ttl)
                delete []ttl;
            ttl = new uint16_t[n];
        }

        num_leds = n;

        for(int i=0; i<num_leds; i++) {
            targets[i] = CRGB::Black;
            if(use_ttl)
                ttl[i] = 0;
        }
    }
  
    #if 0
    void step(uint16_t activity) {
        // convert into a delay of 5 to 21
        uint32_t delta = (100 - activity) / 6 + 5;
        uint32_t now = millis();
        if(now - last_update < delta)
            return;
        last_update = now;

        fadeToBlackBy(leds, num_leds, 5+activity);
    }
    #endif
    
    void blur(uint8_t blur_amount) {
        blur1d(targets, num_leds, blur_amount);
    }

    void set(uint16_t led, CRGB color, uint16_t lifespan) {
        if(led >= num_leds)
            return;

        targets[led] = color;
        ttl[led] = lifespan;
    }

    void blend(CRGB &tgt, uint16_t idx, uint8_t blend_amount=10) {
        if(use_ttl) {
            if(!ttl[idx]) {
                targets[idx] = CRGB::Black;
                #warning should this be a return? does blending black darken or is it nop?
                return;
            }
            else
                ttl[idx]--;
        }

        //if(targets[idx] == CRGB::Black)
        //    return;
        
        nblend(tgt, targets[idx], blend_amount);
    }

    #if 1
    void blend_additive(CRGB &tgt, uint16_t idx, uint8_t blend_amount=10) {
        #warning  dig into the best way to do this
        // XXX rgb2hsv_approximate doesn't appear to work

        // blend the huess but add the values
        CHSV dst = rgb2hsv_approximate(tgt);
        CHSV src = rgb2hsv_approximate(targets[idx]);

        //dst.hue = (dst.hue + src.hue)/2;
        //if((uint16_t)dst.value + (uint16_t)src.value > 255)
        //    dst.value = 255;
        //else
        //    dst.value += src.value;

        tgt = dst;
    }
    #endif

    void blend_color() {
    }

    void overwrite_color() {
    }

    void fade(uint8_t fade_amount) {
        fadeToBlackBy(targets, num_leds, fade_amount);
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
    //void on_pir();
    void background_update(meta_state_t &state);

    void find_mids();
    bool near_mids(int i);
    void do_rainbow();
    void do_basic_ripples(uint16_t activity);
    void do_high_energy_ripples(uint16_t activity);
    void do_wave(uint8_t brightness, int refresh);
    void fade_all(uint16_t amount);

    // Gardener told us to start a pulse
    void handle_remote_pulse(uint32_t color, uint8_t fade, uint16_t spread, uint32_t delay);

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
  