#pragma once
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <FastLED.h>
#include <map>
#include "common/common.h"
#include "common/mux.h"
#include "patterns.h"

//#define FIND_SENS 1
//#define LED_TEST 1
//#define LOG_LATENCY 1

#define LOG_TIMEOUT 500 // in ms

#define TRANS_TIMEOUT 10000

#define INIT_TRIG_THOLD 180 
#define INIT_TRIG_MAX 600
#define TRIG_SPREAD_MIN 175
#define TRIG_DECAY_DELTA_MIN 100 // ms
#define TRIG_DECAY_DELTA_MAX 500 // ms
#define MIN_TRIG_FOR_STATE_CHANGE 2
#define STATE_FALL_TIMEOUT 60000 // this many ms inactive and we transition down

#define MAX_RIPPLES_RAND 4

#define LED_PIN1    15
#define LED_PIN2    14
#define LED_PIN3    13
#define LED_PIN4    12
#define LED_PIN5    11
#define LED_PIN6    10
#define LED_PIN7    9
#define LED_PIN8    8

//#define MODE2 6

#define NUM_LEDS1   2*144 
#define NUM_LEDS2   2*144 
#define NUM_LEDS3   120 // 144
#define NUM_LEDS4   120 // 144
#define NUM_LEDS5   120 // 144
#define NUM_LEDS6   120 // 144

#define MAX_BRIGHT  255
#define LED_TYPE    WS2812
#define COLOR_ORDER RGB

#define DEF_HUE    225
#define DEF_SAT    255
#define DEF_BRIGHT 50
#define MAX_SPREAD num_leds/2

// For convenience with the state tracking
#define MAX_STRIPS 16
#define MAX_STRIP_LENGTH 4*144
#define MAX_MUX_IN 16

#define N_SAMPLES 4
struct ir_input_t {
  uint16_t samples[N_SAMPLES];
  uint8_t idx = 0;
  uint32_t val = 0;

  ir_input_t();
  void update(uint16_t v);
};

struct log_throttle_t {
  uint32_t last = 0;
  uint32_t log_timeout = LOG_TIMEOUT;
  bool should_log() {
    uint32_t now = millis();
    if(now - last < log_timeout)
      return false;

    last = now;
    return true;
  }
};

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
  uint32_t get_min();
  uint32_t get_max();
  uint32_t get_thold();
  void decay(); // slowly close the range
};

struct sensor_t {
  uint16_t location = 0; // LED index
  uint32_t start = 0; // Start of last triggered state
  uint16_t score = 0; // score > 0 when triggered
  int mux_pin = 0; 
  Mux_Read *mux = NULL;
  log_throttle_t log;
  min_max_range_t minmax;

  bool pending_pulse = false;

  void init(Mux_Read *m, int mpin, uint16_t l) {
    mux = m;
    mux_pin = mpin;
    start = 0;
    location = l;
    minmax.pin = mpin;
  }

  void update(uint32_t now);
};

struct strip_t {
  uint8_t pattern = 0;
  uint16_t num_leds = 0;
  uint32_t last_update = 0;

  CRGB *leds = NULL,
       *layer_tracers = NULL,
       *layer_waves = NULL,
       *transition = NULL,
       *targets = NULL;

  uint16_t *lifespans = NULL; // for the original pattern

  waves_t waves;
  wave_pulse_t wave_pulse;

  tracer_t tracers_sens[MAX_MUX_IN+1];
  tracer_t tracers_rand[MAX_RIPPLES_RAND];

  rainbow_t rainbow;
  blobs_t blobs;
  blob_asc_t blob_asc;
  climb_white_t white;

  sensor_t sensors[MAX_MUX_IN];
  uint16_t nsens = 0,
           n_tracers = 0;

  bool in_pulse = false,
       is_center = false;

  log_throttle_t log;
  SemaphoreHandle_t mtx = xSemaphoreCreateMutex();

  void init(CRGB *l, uint16_t nleds, bool is_ctr=false);

  // Each strip needs to know which sensors to query and there LED offset
  void add_input(Mux_Read *mux, uint8_t mx_pin, uint16_t led);

  ~strip_t() {
    // So not necessary but hurts to omit...
    if(layer_tracers)
      delete []layer_tracers;
    if(layer_waves)
      delete []layer_waves;
    if(targets)
      delete []targets;
    if(lifespans)
      delete []lifespans;
    if(transition)
      delete []transition;
  }

  void step();
  CRGB trig_target_color();
  void update_triggered(uint32_t now);
  void update_triggered(uint32_t now, int i);
  void find_mids();
  bool near_mids(int i);
  void do_rainbow();
  void do_basic_ripples(uint16_t activity);
  void do_high_energy_ripples(uint16_t activity);
  void do_wave(uint8_t brightness, int refresh);
  void go_white();

  void handle_pulse();
  void handle_low_activity();
  void handle_low_power();
  //void handle_pulse_white();

  // For all of the number crunching we can do on the other core
  void background_calc();
};

#define N_STATE_SAMPLES 5
struct state_t {
  uint32_t t_activated = 0,
           max_duration = 100, // milliseconds this change should last
           rest = 150,
           rest_until = 0; //how long to wait before we can trigger this state again
  uint8_t blend = 50; // amount to blend LEDs
  bool active = false,
       do_once = false,
       need_retrigger = false;

  uint32_t score = 0,
        thold_act = 5,
        thold_deact = 2,
        max_score = 0;

  uint32_t avg = 0;
  uint16_t n_samples = 1;

  void update_avg();
  bool can_trigger();
  bool is_triggered();
  void trigger();
  void untrigger();
  void deactivate();
  bool resting();
};

#define T_MAX_WHITE 5000 // after 5 seconds, assume sun is out
#define T_LOW_ACTIVITY 10000
struct meta_state_t {
  uint16_t score = 0;
  uint16_t num_sens = 0,
           total = 0;
  Mux_Read *mux = NULL;
  state_t pulse, 
          low_power, 
          active;
          //white;
  uint32_t last_active = 0;
  bool in_pulse = false;
  uint32_t t_pulse = 0;
  int pattern = 0;

  void init(Mux_Read *m) {
    mux = m;
    last_active = millis();

#if 0
    white.rest = 100;
    white.max_duration = 500;
    white.blend = 10;

    pulse.rest = 500;
    pulse.max_duration = 500;
    pulse.blend = 10;

    low_power.rest = 5000;
    low_power.delay = 5000; // should be way higher

    active.max_duration = 10000; // should prob be way longer
    active.rest = 5000;
#endif

    pulse.thold_act = 50;
    pulse.rest = 1000;
    pulse.max_duration = 500;
    pulse.max_score = 200;
    pulse.thold_deact = 10;

    active.thold_act = 5;
    active.thold_deact = 2;
    active.max_score = 200;
    
    //white.thold_deact = 3;
    low_power.thold_act = 200;
    low_power.thold_deact = 50;
    low_power.max_score = 400;
  }

  bool do_pulse();
  void update();
};

#if 0
struct cmd_t {
  uint32_t ts = 0;
  uint16_t type = 0;
  uint16_t len = 0;
  // char *data;
};

enum state_t {
  STATE_INIT,
  STATE_RUNNING
};
#endif

