#include "stepper.h"

uint32_t Accel::next() {
  // Speed is in pulses/sec
  // Accel val is in pulses/sec**2
  // Use t_move_start to figure out what I current speed should be 
  // Convert pulse/sec back to delay
  
  uint32_t now = micros();
  float lapsed = now - t_move_started;

  // we're in micros, so make lapsed more manageable
  lapsed /= ACCEL_CONST_DIV;

  // desired speed in pulse/sec
  float d = lapsed / accel_0 + lapsed*lapsed / accel_1;
  
  //float d = lapsed; // lapsed * accel_val;

  Serial.printf("Accel: %f + %f = %f ... ", lapsed / accel_0, lapsed*lapsed / accel_1, d);

  if(state == ACCEL_UP) {
    //d *= accel_val;
    d = -d;
    //Serial.printf(" [accel]: %f ", d);
  }
  //else {
  //  d *= decel_val;
    //Serial.printf(" [decel]: %f ", d);
  //}

  //delay_current = max(delay_min, delay_current);
  //delay_current = min(delay_max, delay_current);
  uint32_t ret = max(delay_min, delay_current + d);
  ret = min(delay_max, ret);
  Serial.printf("delay current: %u, returning: %u\n", delay_current, ret);
  return ret;
}