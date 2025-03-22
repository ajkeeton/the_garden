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
  STEP_OPEN,  // Opposite above
};

#define DEFAULT_MODE STEP_CLOSE
#define DEFAULT_MODE_NEXT STEP_OPEN

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
  uint32_t delay_current = 0,
           delay_target = 0;
  uint32_t delay_min = 0,
           delay_max = 0;

  uint32_t point_to_accel = 0,
           point_to_decel = 0,
           point_start = 0;
  ACCEL_STATE state = ACCEL_NEUTRAL;
  
  void init(uint32_t dmin, uint32_t dmax) {
    delay_min = dmin;
    delay_max = dmax;
    delay_current = dmax;
  }

  void set_target(uint32_t tgt) {
    delay_target = tgt;
    delay_current = delay_max;
    state = ACCEL_UP;
  }

  void reset() {
    delay_target = delay_min;
    delay_current = delay_max;
    state = ACCEL_UP;
  }

  uint32_t next(uint32_t cur_num_steps) {
    // XXX Crappy not really accelleration / deceleration

    //Serial.printf("wtf: %u and %d\n", delay_current, cur_num_steps - point_to_accel);

    if(cur_num_steps < point_to_accel)
      delay_current--;
    
    if(cur_num_steps > point_to_decel)
      delay_current++;
    
    delay_current = max(delay_current, delay_min);
    delay_current = min(delay_current, delay_max);
    //delay_current -= n;

    //Serial.printf("cur: %u and %u\n", delay_current, cur_num_steps);

    return delay_current;

    /*
    delay_current = delay_current - ((2.0 * (float)delay_current) / ((4.0 * (float)steps) + 1));

    //delay_current = max(delay_current, delay_min);
    //delay_current = min(delay_current, delay_max);

    if(delay_current != 0 && delay_current != delay_min && delay_current != delay_max)
      Serial.printf("cur: %u vs %u and %u\n", delay_current, delay_min, delay_max);
    //delay(500);
    */

    /*
    // https://www.littlechip.co.nz/blog/a-simple-stepper-motor-control-algorithm
    switch(state) {
      case ACCEL_UP:
          if(delay_current > delay_target) {
            delay_current = delay_current * (4 * cur_num_steps - 1) / (4 * cur_num_steps + 1);
            //delay_current = max(delay_min, delay_current);
            if(delay_current < delay_target)
              delay_current = delay_target;
            else
              Serial.printf("accel cur: %u\n", delay_current);
          }
          else 
            // XXX With the current implementation this is redundant ... 
            // once the delay has shrunk to the target we stop changing it anyway
            state = ACCEL_NEUTRAL;
          break;
      case ACCEL_DOWN:
          if(delay_current < delay_max) {
            delay_current = delay_current * (4 * cur_num_steps + 1) / (4 * cur_num_steps - 1);
            if(delay_current > delay_max) 
              delay_current = delay_max;
            
            Serial.printf("decel cur: %u vs %u\n", delay_current, delay_max);
          }
          break;
      default:
          break;
    }
    */

    return delay_current;
  }
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
          pos_end = INT_MAX,  // Set when we hit the endpoint
          pos_to_decel = 0,
          cur_num_steps = 0;
  
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
  void randomize_delay();
  void set_forward(bool f);
  void set_target(int32_t tgt, uint32_t tp = 0, uint32_t td = 0);

  void run();
};
