#pragma once

#include "common.h"
#include "src/mux.h"

extern Mux_Read mux;

class Stepper {
public:
  uint16_t idx = 0;

  uint32_t tlast = 0,   // time of last step
           tdelay = 0,  // delay before changing step
           tpause = 0,  // time to pause before making a change
           tignore_limit = 0,
           delay_min = DELAY_MIN,
           delay_max = DELAY_MAX; 

  int32_t position = 0, // Tracks position so we can accel/decel strategic points
          pos_end = INT_MAX,  // Set when we hit the endpoint
          pos_tgt = 0,
          cur_num_steps = 0,
          pos_decel = 0;
  
  bool forward = true;
  int state_step = 0;

  bool last_dir = true,
       was_on = false;

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
    tdelay = dmin;// * 1000;
    delay_min = dmin;// * 1000;
    delay_max = dmax; // * 1000;

    was_on = true;
    set_onoff(false);

    set_target(-INT_MAX); // set target to low val so we start by discovering 
                          // find the lower limit
  }

  bool check_limit(int pin) {
    //Serial.printf("Limit %d: %d\n", pin, mux_read_raw(pin));
    return mux.read_switch(pin);
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

  void choose_next() {
    choose_next_rand_walk();
  }

  void choose_next_rand_walk() {
    // set_onoff(false); For the stress test, only turn off motor when we're fully extended (pos_end)

    cur_num_steps = 0;
    //bool on_limit = false;

    tpause = 1000000;
    tdelay = delay_min; // random(delay_min, delay_max);

    if(position == pos_end) {
      set_target(0);
      //set_onoff(false);
      Serial.printf("%d: At end\n", idx);
      //delay(1000);
    }
    else {
      set_target(pos_end);
      Serial.printf("%d: At start, cooling off 1 sec\n", idx);
      //set_onoff(false);
      //delay(1000);
    }

    tlast = micros();
  }

  void set_forward(bool f) {
    if(forward != f) {
      tpause = max(30000, tpause); // min 30 ms before reversing
      cur_num_steps = 0;
    }

    forward = f;
    // Serial.printf("direction: %d\n", forward);
    if(forward) {
      digitalWrite(pin_dir, HIGH);
    }
    else {
      digitalWrite(pin_dir, LOW);
    }
  }

  void set_target(int32_t tgt) {
    pos_tgt = tgt;

    if(pos_tgt > position)
      pos_decel = (pos_tgt - position) * .9;
    else
      pos_decel = (position - pos_tgt) * .9;
  
    if(pos_tgt < position)
      set_forward(false);
    else
      set_forward(true);
    
    cur_num_steps = 0;
    
    Serial.printf("%d: Position: %d, New target: %d End: %d fwd/back: %d\n", idx, position, pos_tgt, pos_end, forward);
  }

  void run();
};
