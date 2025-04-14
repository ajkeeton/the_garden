#pragma once

enum ACCEL_STATE {
  ACCEL_UP,
  ACCEL_DOWN,
  ACCEL_NEUTRAL // Use to disable acceleration/deccel
};

class Accel {
public:
  uint32_t delay_current = 0,
           delay_init = 0,
           count = 0;

  uint32_t time_to_target = 0; // set when we start to decel

  uint32_t delay_min = 0,
           delay_max = 0,
           def_default_min = 0,
           def_default_max = 0;
  /*
  uint32_t point_to_accel = 0,
           point_to_decel = 0,
           point_start = 0;
  */

  uint32_t t_move_started = 0,
           t_last_update = 0,
           t_pause_for = 0; // a pause before we start moving and updating the delay

  //uint32_t accel_val = 1,
  //         decel_val = 1;

  float accel_0 = 0.00001,
        accel_1 = 100;

  //float step_size = 0;
  //uint32_t step_counts = 0, 
  int32_t steps_to_target = 0;

  ACCEL_STATE state = ACCEL_NEUTRAL;
  
  void init(uint32_t dmin, uint32_t dmax) {
    def_default_min = delay_min = dmin;
    def_default_max = delay_max = dmax;
    delay_init = delay_current = dmax;
    state = ACCEL_UP;
  }

  void set_target(uint32_t midpoint, uint32_t dmax, uint32_t dmin, float a0) {
    accel_0 = a0;
    delay_current = delay_init = dmax;
    count = 0;
    delay_max = dmax; // XXX Refactor -  probably no longer need delay_max
    delay_min = dmin;
    steps_to_target = max(1, midpoint);
    state = ACCEL_UP;
    time_to_target = t_last_update = t_move_started = 0;
  }

  void set_decel() {
    delay_init = delay_current;
    steps_to_target = -steps_to_target;
    time_to_target = micros() - t_move_started;
    t_move_started = micros();
    state = ACCEL_DOWN;
  }

  void set_pause_ms(uint32_t t) {
    t_pause_for = t * 1000; // convert to micros
    t_last_update = micros();
    t_move_started = 0;
  }

  void set_pause_min() {
    // Min time to pause before changing directions
    t_pause_for = min(10000, t_pause_for);
    t_last_update = micros();
    t_move_started = 0;
  }

  bool is_ready() {
    uint32_t now = micros();

    // NOTE: using subtraction as follows will account for integer overflow
    if(now - t_last_update < t_pause_for)
      return false;
    
    t_pause_for = 0;
    
    if(!t_move_started) {
      t_move_started = now;
      t_last_update = now;
      return false;
    }

    if(now - t_last_update < delay_current)
      return false;

    t_last_update = now;
    return true;
  }

  int32_t next();
  // Uses a plateau function to acceleration
  int32_t next_plat();

private:
  // Keep the delay between min and max
  int32_t _constrain(int32_t x);
};
