#pragma once

#include "common.h"
#include "src/mux.h"

extern Mux_Read mux;

enum STEP_STATE {
  STEP_DEFAULT,
  STEP_FIND_ENDPOINT_1,
  STEP_FIND_ENDPOINT_2,
  STEP_WIGGLE_START,
  STEP_WIGGLE_END,
  STEP_RANDOM_WALK,
  STEP_SWEEP,
  STEP_CLOSE, // Close fingers to test range, confirm direction, etc
  STEP_OPEN,  // Opposite above, sets '0' based on limit switch
  STEP_RELAX, // 90%'ish full open stretch
};

#define DEFAULT_MODE STEP_OPEN
//#define DEFAULT_MODE_NEXT STEP_SWEEP

#define STEPPER_OFF false
#define STEPPER_ON true

enum TRIGGER_STAT {
    TRIGGER_OFF,
    TRIGGER_ON,
    TRIGGER_WAIT,
};

class Limit_State {
public:
  int val_low = 0, 
      val_high = 0,
      pin_low = 0,
      pin_high = 0;
  bool triggered_low = false,
       triggered_high = false;
  uint32_t last_trigger_on = 0;

  //bool using_tmc2209 = false;
  //bool *forward = NULL; // If we're using the diag pin then we have to know 
                        // which direction we're going when it gets triggered

  // Using "classic" high and low switches
  void init(int low, int high) {
    pin_low = low;
    pin_high = high;
  }

  /*
  // Using TMC2209 pin
  void init(int diag_in, bool *fwd) {
    using_tmc2209 = true;
    pin_low = diag_in;
    forward = fwd;
  }
  */

  TRIGGER_STAT check_triggered() {
    int clow = !mux.read_switch(pin_low);
    int chigh = !mux.read_switch(pin_high);

    // XXX
    // XXX TODO: abstract out the individual switches

    // Handle case we were already triggered
    // Need to give motors time to get off the limit switch
    // and also debounce
    if(triggered_low) {
       uint32_t now = millis();
      // We were previously triggered
      // Check if we're no longer
      if(!clow) {
        // We're no longer triggered
        // Debounce
        if(now - last_trigger_on > 10) {
          triggered_low = false;
        }
      }
      else {
        last_trigger_on = now;
      }
      
      return TRIGGER_WAIT;
    }
    if(triggered_high) {
      uint32_t now = millis();

      if(!chigh) {
        // We're no longer triggered
        // Debounce
        if(now - last_trigger_on > 10) {
          triggered_high = false;
        }
      }
      else {
        last_trigger_on = now;
      }
      return TRIGGER_WAIT;
    } 

    triggered_low = clow;
    triggered_high = chigh;

    if(triggered_low || triggered_high) {
      last_trigger_on = millis();
      return TRIGGER_ON;
    }

    return TRIGGER_OFF;
  }

private:
/*
  void check_low() {
      bool t = mux.read_switch(pin_low);
      //Serial.printf("mux val: %u\n", t);
      if(using_tmc2209) {
        // If we're not triggered, then both sides are clear
        if(!t) {
          triggered_low = triggered_high = false;
        }
        else {
          if(*forward) {
            if(!triggered_low) Serial.printf("Limit low triggered");
            triggered_low = true;
          }
          else {
            if(!triggered_high) Serial.printf("Limit high triggered");
            triggered_high = true;
          }
        }
      }
      else 
        triggered_low = t;
  }

  void check_high() {
      if(!using_tmc2209)
        triggered_high = mux.read_switch(pin_high);
  }
  */
};

enum ACCEL_STATE {
  ACCEL_UP,
  ACCEL_DOWN,
  ACCEL_NEUTRAL // Use to disable acceleration/deccel
};

class Accel {
public:
  uint32_t delay_current = 0;
  uint32_t delay_min = 0,
           delay_max = 0,
           def_default_min = 0,
           def_default_max = 0;

  /*
  uint32_t point_to_accel = 0,
           point_to_decel = 0,
           point_start = 0;
  */

  uint32_t t_move_started = 0;
  uint32_t accel_val = 1,
           decel_val = 1;

  ACCEL_STATE state = ACCEL_NEUTRAL;
  
  void init(uint32_t dmin, uint32_t dmax) {
    def_default_min = delay_min = dmin;
    def_default_max = delay_max = dmax;
    delay_current = dmax;
    state = ACCEL_UP;
  }

  void set_target(uint32_t a, uint32_t d = 0) {
    if(!d) d = a;
    accel_val = a;
    decel_val = d;

    delay_current = delay_max;
    state = ACCEL_UP;
    t_move_started = micros();
  }

  void reset() {
    init(def_default_min, def_default_max);
  }

  void set_decel(uint32_t cur) {
    // If it's time to decel, reset the move start and calc the inverse
    state = ACCEL_DOWN;
    delay_current = cur;
    t_move_started = micros();
  }

  uint32_t next();
};

class Stepper {
public:
  uint16_t idx = 0; // Which stepper this is, used for logging

  uint32_t tlast = 0,         // time of last step
           t_pulse_delay = 0, // delay before changing step
           tpause = 0,        // time to pause before moving again
           pulse_delay_min = DELAY_MIN, // Minimum pulse delay
           pulse_delay_max = DELAY_MAX; // Max pulse default

  int32_t position = 1,
          pos_tgt = -INT_MAX, // Our target position
          pos_end = DEFAULT_MAX_STEPS,
          pos_to_decel = 0;
  
  bool forward = true,
       was_on = false;
  int step_pin_val = 0;
  
  STEP_STATE state = STEP_DEFAULT,
             state_next = STEP_DEFAULT;

  int pin_step = 0,
      pin_dir = 0,
      pin_enable = 0;

  int val_forward = HIGH,
      val_backward = LOW;

  Limit_State limits;
  Accel accel;

  void init(int i, int en, int step, int dir, int lsl, int lsh, 
            int dmin = DELAY_MIN, int dmax = DELAY_MAX) {
    idx = i;
    pin_enable = en;
    pin_step = step;
    pin_dir = dir;

    t_pulse_delay = dmin;// * 1000;
    pulse_delay_min = dmin;// * 1000;
    pulse_delay_max = dmax; // * 1000;

    was_on = true;
    set_onoff(STEPPER_OFF);
    state = STEP_FIND_ENDPOINT_1;

    limits.init(lsl, lsh);
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

        tpause = max(20000, tpause);
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
  void randomize_delay();
  void set_forward(bool f);
  void set_target(int32_t tgt);

  void run();
};
