#pragma once

#include "common.h"
#include "common/mux.h"
#include "accel.h"

#define STEP_LOG_DELAY 1000

extern Mux_Read mux;

enum STEP_STATE {
  STEP_INIT,
  //STEP_FIND_ENDPOINT,
  STEP_WIGGLE_START,
  STEP_WIGGLE_END,
  STEP_RANDOM_WALK,
  STEP_SWEEP,
  STEP_CLOSE, // Close fingers to test range, confirm direction, etc
  STEP_OPEN,  // Opposite above, sets '0' based on limit switch
  STEP_TRIGGERED_INIT, // Open, prepare to grab
  STEP_GRAB,
  STEP_GRAB_WIGGLE,
  STEP_RELAX, // 90%'ish full open stretch
};

#define DEFAULT_MODE STEP_INIT
#define DEFAULT_MODE_NEXT STEP_SWEEP

#define STEPPER_OFF false
#define STEPPER_ON true

enum TRIGGER_STAT {
    TRIGGER_OFF,
    TRIGGER_ON,
    TRIGGER_WAIT,
};

struct step_settings_t {
  uint32_t pause_ms = 50,
           min_delay = DELAY_MIN,
           max_delay = DELAY_MAX,
           min_pos = 0,
           max_pos = DEFAULT_MAX_STEPS;
  float accel = 0.000005;
};

class Limit_State {
public:
  int val_low = 0, 
      val_high = 0,
      pin_low = 0;
      // pin_high = 0;
  bool triggered_low = false;
       //triggered_high = false;
  uint32_t last_trigger_on = 0;

  //bool using_tmc2209 = false;
  //bool *forward = NULL; // If we're using the diag pin then we have to know 
                        // which direction we're going when it gets triggered

  // Using "classic" high and low switches
  void init(int low) {
    pin_low = low;
    //pin_high = high;
  }

  /*
  // Using TMC2209 pin
  void init(int diag_in, bool *fwd) {
    using_tmc2209 = true;
    pin_low = diag_in;
    forward = fwd;
  }
  */

  TRIGGER_STAT check_triggered(int pin, bool &trigger_state) {
    int ls = mux.read_switch(pin);

    if(trigger_state) {
      uint32_t now = millis();

    // We were previously triggered
    // Check if we're no longer
      if(!ls) {
        // We're no longer triggered
        // sort of debounce
        if(now - last_trigger_on > 10) {
          trigger_state = false;
        }
      }
      else {
        last_trigger_on = now;
      }

      return TRIGGER_WAIT;
    }

    trigger_state = ls;

    if(trigger_state) {
      last_trigger_on = millis();
      return TRIGGER_ON;
    }

    return TRIGGER_OFF;
  }

  TRIGGER_STAT check_triggered() {
    TRIGGER_STAT stat_low = check_triggered(pin_low, triggered_low);
    return stat_low;
    
    #if 0
    TRIGGER_STATE stat_high = check_triggered(pin_high, triggered_high);
    ...
    #endif
  }
};

class Stepper {
public:
  uint16_t idx = 0; // Which stepper this is, used for logging

  uint32_t pulse_delay_min = DELAY_MIN, // Minimum pulse delay
           pulse_delay_max = DELAY_MAX; // Max pulse default

  int32_t position = 1,
          pos_tgt = -INT_MAX, // Our target position
          pos_end = DEFAULT_MAX_STEPS;

  bool forward = true,
       was_on = false;
  int step_pin_val = 0;
  
  STEP_STATE state = DEFAULT_MODE,
             state_next = DEFAULT_MODE_NEXT;

  int pin_step = 0,
      pin_dir = 0,
      pin_enable = 0;

  int val_forward = HIGH,
      val_backward = LOW;

  uint32_t last_close = 0,
           last_log = 0;

  step_settings_t settings_on_close,
                  settings_on_wiggle;

  Limit_State limits;
  Accel accel;

  void init(int i, int en, int step, int dir, int lsl, 
            int dmin = DELAY_MIN, int dmax = DELAY_MAX) {
    idx = i;
    pin_enable = en;
    pin_step = step;
    pin_dir = dir;

    pulse_delay_min = dmin;// * 1000;
    pulse_delay_max = dmax; // * 1000;

    was_on = true;
    set_onoff(STEPPER_OFF);
    state = DEFAULT_MODE;

    limits.init(lsl);
    accel.init(dmin, dmax);
  }

  void set_backwards() {
    val_forward = LOW;
    val_backward = HIGH;
    set_forward(true);
  }

  void set_onoff(bool onoff) {
    if(onoff) {
      if(!was_on) {
        was_on = true;
        #if USE_PWM_DRIVER
        step_en.setPWM(pin_enable, 4096, 0);
        #else
        digitalWrite(pin_enable, LOW);
        #endif

        accel.set_pause_min();
      }
    }
    else {
      if(was_on) {
        was_on = false;
        #if USE_PWM_DRIVER
        step_en.setPWM(pin_enable, 0, 4096);
        #else
        digitalWrite(pin_enable, HIGH);
        #endif
      }
    }
  }

  void choose_next();
  void choose_next_sweep();
  void choose_next_rand_walk();
  void choose_next_wiggle();
  void choose_next_wiggle(int32_t lower, int32_t upper);
  void randomize_delay();
  void set_forward(bool f);
  void set_target(int32_t tgt);
  void set_target(int32_t tgt, const step_settings_t &ss);
  void trigger_close();
  void run();
};
