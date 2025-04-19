#pragma once

#define DEF_RIPPLE_BRIGHTNESS 255

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
struct tracer_v2_t {
  CRGB *leds = NULL;
  uint8_t brightness = DEF_RIPPLE_BRIGHTNESS;
  uint8_t color = 0;
  uint16_t init_pos = 0, 
           num_leds = 0;
  int16_t pos = 0,
          spread = 0;

  // int8_t velocity = 0;
  uint8_t // life = 0,
          // max_life = 0,
          fade = 0;

  bool exist = false;
  uint32_t t_last_update = 0,
           t_last_trigger = 0,
           t_update_delay;
  
  void init(CRGB *l, uint16_t nleds, uint16_t pos) {
    leds = l;
    num_leds = nleds;
    init_pos = pos;
  }

  bool trigger(uint16_t lidx, uint32_t score, uint32_t score_duration) {
    if(exist)
      // let the last one finish first
      return false;

    uint32_t now = millis();
    // The delay is based on the duration. Longer duration, shorter delay
    if(score_duration > MAX_SCORE_DURATION)
      score_duration = MAX_SCORE_DURATION; // cap it at 10 seconds
    //t_update_delay = map(score_duration, 0, 10000, 2000, 500);

    //if(now - t_last_trigger < t_update_delay)
    //  return;

    t_last_trigger = now;

    color = CRGB::White; // millis() & 255; // random8() & 255;

    pos = init_pos = lidx;

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
    spread =  map(score_duration, 0, MAX_SCORE_DURATION, 5, 30);
    t_last_update = 0;

    exist = true;

    Serial.printf("Tracer starting using score %lu\n", score);

    return true;
  }

  void step();
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

struct waves_t {
  uint8_t wave_offset = 0;
  uint32_t last_update = 0;
  CRGB *leds = NULL;
  uint16_t num_leds = 0;

  waves_t() {
    wave_offset = random8();
  }

  void init(CRGB *l, uint16_t n) {
    leds = l;
    num_leds = n;
  }

  void step(uint8_t brightness, uint16_t delay, uint16_t wave_mult);
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

    // One blob every 144 leds
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
