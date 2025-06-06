#pragma once

#define DEF_RIPPLE_BRIGHTNESS 255
#define TRANS_TIMEOUT 10000

#include <FastLED.h>

struct rainbow_t {
  uint16_t nleds = 0;
  uint8_t hue = 0;
  CRGB *leds = NULL;
  uint32_t last_update = 0;

  void init(uint16_t n) {
    nleds = n;
    if(leds)
      delete []leds;

    leds = new CRGB[n];
  }
  ~rainbow_t() {
    if(leds)
      delete []leds;
  }

  CRGB get(uint16_t i, uint8_t brightness) {
    if(brightness) {
      CHSV hsv = rgb2hsv_approximate(leds[i]);
      hsv.value = brightness;
      return hsv;
    }

    return leds[i];
  }

  void update();
};

#define MAX_SCORE_DURATION 10000
struct tracer_v2_pulse_t {
  CRGB *leds = NULL;
  uint8_t brightness = DEF_RIPPLE_BRIGHTNESS;
  uint32_t color = 0;
  uint16_t init_pos = 0, 
           num_leds = 0;
  uint16_t pos = 0,
          spread = 0;
  bool reverse = false;

  int8_t direction = -1;

  // int8_t velocity = 0;
  uint8_t // life = 0,
          // max_life = 0,
          fade = 0;

  bool exist = false;
  uint32_t t_last_update = 0,
           t_last_trigger = 0,
           t_update_delay = 0;

  tracer_v2_pulse_t(
      CRGB *l, uint16_t nl, uint16_t lidx, uint32_t score, uint32_t score_duration, bool rev=false) {
    reverse = rev;
    leds = l;
    num_leds = nl;
    pos = init_pos = lidx;

    uint32_t now = millis();
    // The delay is based on the duration. Longer duration, shorter delay
    if(score_duration > MAX_SCORE_DURATION)
      score_duration = MAX_SCORE_DURATION; // cap it at 10 seconds
    //t_update_delay = map(score_duration, 0, 10000, 2000, 500);

    //if(now - t_last_trigger < t_update_delay)
    //  return;

    t_last_trigger = now;

    color = CRGB::White; // millis() & 255; // random8() & 255;

    // score determines the fade
    // at 100%, fade is low
    if(score > 100) score = 100;
    fade = map(0, 100, 100, 2, score);

    // calc a blur based on score?

    if(score > 100)
      score = 100;

    // recalc the update delay to be per frame
    // t_update_delay = map(score, 0, 100, 40, 2);
    t_update_delay = 5; // map(score_duration, 0, MAX_SCORE_DURATION, 20, 1);
    spread =  map(score_duration, 0, MAX_SCORE_DURATION, 1, 20);
    t_last_update = 0;

    exist = true;

    Serial.printf("Tracer starting using score %lu & duration: %lu\n", score, score_duration);
  }

  tracer_v2_pulse_t(CRGB *l, uint16_t nl, uint32_t c, uint8_t f, uint16_t s, uint32_t d) {
    pos = init_pos = 0;
    num_leds = nl;
    leds = l;
    t_update_delay = d;
    color = c;
    fade = f;
    spread = s;
    t_update_delay = d;
    t_last_update = 0;
    exist = true;
    reverse = true;

    //Serial.printf("Remote tracer starting (color: %u, fade: %u, spread: %lu, d: %lu)\n", c, f, s, d);
    //Serial.println("jk"); exist=false;
  }

  void step();
};

#include <list>

#define MAX_TRACER_PULSES 8
#define T_THROTTLE_MS 1000
struct tracer_v2_t {
  CRGB *leds = NULL;
  uint16_t num_leds = 0;
  uint32_t t_last = 0;

  # warning a small array that we just iterate over to check for "exist" is probably better than a small quueue
  std::list<tracer_v2_pulse_t> nodes;

  void init(CRGB *l, uint16_t nleds) {
    leds = l;
    num_leds = nleds;
  }

  void step() {
    for(auto it = nodes.begin(); it != nodes.end();) {
      it->step();
      if(!it->exist) {
        it = nodes.erase(it);
        // not sure if the iterator is still valid, just break for now
        break; 
      }
      else
        ++it;
    }
  }

  void trigger(uint16_t lidx, uint32_t score, uint32_t score_duration, bool reverse=false) {
    if(nodes.size() > MAX_TRACER_PULSES) {
      return;
    }

    uint32_t now = millis();
    if(now - t_last < T_THROTTLE_MS)
      return;
    t_last = now;

    //Serial.printf("starting pulse with: %lu, %u, %u\n", lidx, score, score_duration);

    nodes.push_back(tracer_v2_pulse_t(
        leds, num_leds, lidx, score, score_duration, reverse));
  }

  // Currently
  // Only called for reverse tracers
  void trigger(uint32_t c, uint8_t f, uint16_t s, uint32_t d) {
    if(nodes.size() > MAX_TRACER_PULSES) {
      Serial.printf("Too many tracers, ignoring\n");
      return;
    }

    uint32_t now = millis();
    if(now - t_last < T_THROTTLE_MS)
      return;
    t_last = now;

    // Serial.printf("starting reverse pulse with: %lu, %u, %u, %lu\n", c, f, s, d);

    nodes.push_back(tracer_v2_pulse_t(leds, num_leds, c, f, s, d));
  }
};

struct tracer_t {
  CRGB *leds = NULL;
  uint32_t last = 0;
  uint8_t brightness = 0;
  uint8_t color = 0;
  uint16_t init_pos = 0, 
          num_leds = 0;
  int16_t pos = 0;

  int8_t velocity = 0;
  uint8_t life = 0,
          max_life = 0,
          fade = 0;
  bool exist = false;
  uint32_t last_update = 0;
  
  void init(CRGB *l, uint16_t nleds, uint16_t pos) {
    leds = l;
    num_leds = nleds;
    init_pos = pos;
  }

  void reset(uint8_t f, uint8_t max, int8_t vel=1) {
    pos = init_pos;
    velocity = vel;
    life = 0;
    max_life = max;
    exist = true;
    brightness = DEF_RIPPLE_BRIGHTNESS;
    color = millis();
    //color = 1; // random8() & 1;
    fade = f;
  }

  uint16_t get_pos() {
    return pos < num_leds ? pos : num_leds;
  }

  uint16_t get_neg_pos() {
    int16_t d = pos - init_pos;
    int16_t npos = pos - d;
    if(npos < 0)
      return 0;
    if(npos > num_leds) // don't think this could ever happen...
      return num_leds;
    
    return npos;
  }

  void step();
  void step(uint16_t activity);
};

struct animate_waves_t {
  uint8_t wave_offset = 0;
  uint32_t last_update = 0;
  CRGB *leds = NULL;
  uint16_t num_leds = 0;

  animate_waves_t() {
    wave_offset = random8();
  }

  void init(CRGB *l, uint16_t n) {
    leds = l;
    num_leds = n;
  }

  void step(uint8_t brightness, uint16_t delay, uint16_t wave_mult);
};

#warning TODO
struct animate_pulse_white_t {
  uint8_t brightness = 0;
  uint16_t num_leds = 0;
  CRGB *leds = NULL;
  uint32_t last_update = 0;

  void init(CRGB *l, uint16_t n) {
    leds = l;
    num_leds = n;
    last_update = millis();
  }

  void step(uint16_t amount) {}
};

struct wave_pulse_t {
  uint16_t center = 0,
           num_leds = 0,
           spread = 0,
           max_spread = 0,
           color_offset = 0;

  uint32_t last_update = 0,
           start = 0,
           delay_osc = 0,
           max_age = 0;

  CRGB *leds = NULL;

  void init(CRGB *l, uint16_t nl) {
    leds = l;
    num_leds = nl;
    reset();
  }

  void reset() {
    center = 0;
    max_age = random(1000, 7000);
    delay_osc = 5; // random(50, 200);
    start = millis();
    last_update = start;
    spread = 1;
    max_spread = num_leds;
    color_offset = millis();
  }

  void step(uint16_t activity);
};

struct blob_t {
  uint16_t center = 0,
           num_leds = 0,
           spread = 0,
           max_spread = 0;

  uint32_t last_update = 0,
           start = 0,
           max_age = 0;

  CRGB *leds = NULL;

  void init(CRGB *l, uint16_t nl) {
    leds = l;
    num_leds = nl;
    reset();
  }

  void reset() {
    center = random(min(10, num_leds), num_leds > 20 ? num_leds - 10 : num_leds);
    max_age = random(1000, 7000);
    //delay_osc = 5; // random(50, 200);
    start = millis();
    last_update = start;
    spread = 1;
    max_spread = random(min(10, num_leds/15), min(144, num_leds/3));
  }

  void step(uint16_t activity);
};

struct blobs_t {
  blob_t blobs[4];
  uint16_t num_blobs = 0;
  uint32_t last_update = 0;
  CRGB *leds = NULL;
  uint16_t num_leds = 0;

  void init(CRGB *l, uint16_t n) {
    leds = l;
    num_leds = n;

    for(int i=0; i<n; i++)
      leds[i] = CRGB::Black;

    // One blob min every 144 leds
    num_blobs = min(n / 144 + 1, sizeof(blobs)/sizeof(blobs[0]));

    for(int i=0; i<num_blobs; i++) {
      blobs[i].init(l, n);
    }

    last_update = millis();
  }

  void step(uint16_t activity) {
    // activity is a percent

    // convert into a delay of 5 to 21
    uint32_t delta = (100 - activity) / 6 + 5;
    uint32_t now = millis();
    if(now - last_update < delta)
      return;
    last_update = now;

    //activity = activity/50+1;

    // fadeToBlackBy(leds, num_leds, 5+activity);

    //Serial.println(activity);
    for(int i=0; i<num_blobs; i++) {
      //for(int j=0; j<activity; j++)
      blobs[i].step(activity);
    }

    blur1d(leds, num_leds, 160);
  }
};

struct tracer_blob_t {
  CRGB *leds = NULL;
  uint8_t brightness = 0;
  uint8_t color_offset = 0;
  uint16_t init_pos = 0,
          num_leds = 0;
  int16_t pos = 0;

  int8_t velocity = 0;
  uint8_t life = 0,
          max_life = 0,
          fade = 0;
  bool exist = false;
  uint32_t last_update = 0;
  
  void init(CRGB *l, uint16_t nleds, uint16_t pos) {
    leds = l;
    num_leds = nleds;
    init_pos = pos;
  }

  void reset(uint8_t f, int8_t vel=1) {
    pos = init_pos;
    velocity = vel;
    life = 0;
    exist = true;
    brightness = 255;
    color_offset = millis();
    fade = f;
  }

  uint16_t get_pos() {
    return pos < num_leds ? pos : num_leds;
  }

  void step(uint16_t activity);
};

struct blob_asc_t {
  tracer_blob_t tblob[4];
  uint16_t num_blobs = 0;
  CRGB *leds = NULL;
  uint16_t num_leds = 0;
  uint32_t last_update = 0;

  void init(CRGB *l, uint16_t n) {
    leds = l;
    num_leds = n;

    for(int i=0; i<num_leds; i++)
      leds[i] = CRGB::Black;
  
    // One blob every 144 leds
    num_blobs = min(n / 144 + 1, sizeof(tblob)/sizeof(tblob[0]));

    for(int i=0; i<num_blobs; i++) {
      tblob[i].init(l, n, 0);
    }
  }

  void step(uint16_t activity) {
    // convert into a delay of 5 to 21
    uint32_t delta = (100 - activity) / 6 + 5;
    uint32_t now = millis();
    if(now - last_update < delta)
      return;
    last_update = now;

    activity = activity/15+1;

    fadeToBlackBy(leds, num_leds, 5+activity);

    //Serial.println(activity);
    for(int i=0; i<num_blobs; i++) {
      //for(int j=0; j<activity; j++)
        tblob[i].step(activity);
    }
  }
};

struct climb_white_t {
  CRGB *leds = NULL;
  uint16_t num_leds = 0;
  uint32_t last = 0;
  uint16_t pos = 0;
  bool do_trans = false;
  uint32_t time_trans = 0,
           rest_until = 0;

  void init(CRGB *l, uint16_t n) {
    leds = l;
    num_leds = n;

    for(int i=0; i<num_leds; i++)
      leds[i] = CRGB::Black;
  }

  void step(uint16_t activity);
  bool do_transition();
  // bool in_trans();
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