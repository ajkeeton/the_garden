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
};

#define STEPPER_OFF false
#define STEPPER_ON true

class Stepper {
public:
  uint16_t idx = 0;

  uint32_t tlast = 0,   // time of last step
           t_pulse_delay = 0,  // delay before changing step
           tpause = 0,  // time to pause before making a change
           tignore_limit = 0,
           delay_min = DELAY_MIN,
           delay_max = DELAY_MAX; 

  int32_t position = 0, // Tracks position so we can accel/decel strategic points
          pos_start = 0,
          pos_end = INT_MAX,  // Set when we hit the endpoint
          pos_tgt = 0,
          cur_num_steps = 0,
          pos_decel = 0;
  
  bool forward = true,
       was_on = false,
       at_limit = false;
  int step_pin_val = 0;
  
  STEP_STATE state = STEP_DEFAULT,
             state_next = STEP_DEFAULT;

  int pin_step = 0,
      pin_dir = 0,
      pin_enable = 0,
      pin_switch_low = 0,
      pin_switch_high = 0;

  void init(int i, int en, int step, int dir, int lsl, int lsh, int dmin = DELAY_MIN, int dmax = DELAY_MAX) {
    idx = i;
    pin_enable = en;
    pin_step = step;
    pin_dir = dir;
    pin_switch_low = lsl;
    pin_switch_high = lsh;
    t_pulse_delay = dmin;// * 1000;
    delay_min = dmin;// * 1000;
    delay_max = dmax; // * 1000;

    was_on = true;
    set_onoff(STEPPER_OFF);
    state = STEP_FIND_ENDPOINT_1;
  }

  bool check_limit() {
    //Serial.printf("Limit %d: %d\n", pin, mux_read_raw(pin));

    // XXX Not implemented!!
    if(USE_DIAG_PIN)
      // XXX Not implemented!!
      return position == pos_end || position == 0;
    else
      return mux.read_switch(pin_switch_low) || mux.read_switch(pin_switch_high);
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

      tpause = 20000;
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
