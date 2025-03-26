#include "stepper.h"

uint32_t Accel::next() {
  // Speed is in pulses/sec
  // Accel val is in pulses/sec**2
  // Use t_move_start to figure out what I current speed should be 
  // Convert pulse/sec back to delay
  
  uint32_t lapsed = 0,
            speed = 0;
  uint32_t now = micros();

  // desired speed in pulse/sec, divided by constant
  float d = (now - t_move_started) / ACCEL_CONST_DIV;
  //float d = lapsed; // lapsed * accel_val;

  //Serial.printf("accel: %u * %u / 100 = %f .... ", lapsed, accel_val, d);

  if(state == ACCEL_UP) {
    d *= accel_val;
    d = -d;
    //Serial.printf(" [accel]: %f ", d);
  }
  else {
    d *= decel_val;
    //Serial.printf(" [decel]: %f ", d);
  }

  //delay_current = max(delay_min, delay_current);
  //delay_current = min(delay_max, delay_current);
  uint32_t ret = max(delay_min, delay_current + d);
  ret = min(delay_max, ret);
  //Serial.printf("delay current: %u, returning: %u\n", delay_current, ret);
  return ret;
}