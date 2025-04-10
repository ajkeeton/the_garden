#include "stepper.h"

int32_t Accel::_constrain(int32_t x) {
  x = max(delay_min, x);
  return min(delay_max, x);
}

// So handy! https://www.desmos.com/calculator
int32_t Accel::next() {
  count++;

  // Speed is in pulses/sec
  // Accel val is in pulses/sec**2
  // Use t_move_start to figure out what I current speed should be 
  // Convert pulse/sec back to delay
  
  uint32_t now = micros();
  float lapsed = now - t_move_started;

  // desired speed in pulse/sec
  float d = lapsed / accel_0;// + lapsed*lapsed / accel_1;

  // Serial.printf("Accel: %f/%ld = %f\n", d, steps_to_target, d / steps_to_target);

  d /= steps_to_target;

  // Serial.printf("       %ld - %f = %f\n", delay_current, d, delay_current - d);

  delay_current = max(delay_min, delay_init - d);
  delay_current = min(delay_max, delay_current);

  // Serial.printf("       returning: %u\n", ret);
  return delay_current;
}

// Uses a plateau function to acceleration
int32_t Accel::next_plat() {
  count++;

  // total time lapsed since we started moving
  float td = micros() - t_move_started;

  if(count == steps_to_target) {
    // We're at the midway point, use the time elapsed to determine time remaining
    t_move_started = micros();
    time_to_target = td;
    // Serial.printf("Decelerating. TTT: %lu\n", time_to_target);
    return delay_current;
  }
  float et = 0;

  if(count < steps_to_target)
    et = accel_0 * td; // the sweet spot seems to be between 0.00001 to 0.0001
  else
    et = accel_0 * (time_to_target - td);

  float mult = 1 - exp(-et);
  // Mult is now a number between 0 and 1 that will scale the delay

  //if(count < steps_to_target)
  //  mult = 1/mult;

  delay_current = _constrain(delay_max - mult * delay_max);
  //Serial.printf("Accel @ %d:\t%f: %f .... delay: %u\n", count, et, mult, delay_current);
  //Serial.printf("wtf: %lu - %lu = %lu\n", td, time_to_target, td-time_to_target);

  return delay_current;
}

#if 0
int32_t Accel::next() {
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

  Serial.printf("Accel: %f + %f = %f ... ", lapsed / accel_0, lapsed*lapsed / accel_1, d);

  if(state == ACCEL_UP) {
    d = -d;

  uint32_t ret = max(delay_min, delay_current + d);
  ret = min(delay_max, ret);
  Serial.printf("delay current: %u, returning: %u\n", delay_current, ret);
  return ret;
}
#endif

#if 0
int32_t Accel::next() {
  #if 0
  if(step_counts == step_target)
    return delay_current + step_counts * step_size;

  step_counts++;

  //Serial.printf("Accel: %d * %f = %f ... delay current: %u tgt: %u\n ", 
  //  step_counts, step_size, step_counts * step_size, delay_current, step_target);

  return delay_current + step_counts * step_size;
  #endif
  
  // NOTE: intentionally doing the calc even when we pass the target point then clamping
  // simplifies some things with minimal performance impact

  step_counts++;

  int32_t ret = delay_current + step_counts * step_size;
  if(ret < 0 || ret < delay_min)  // the < 0 is due to wrap around and comparing int vs uint
    ret = delay_min;
  if(ret > delay_max)
    ret = delay_max;

  //Serial.printf("Accel: %d * %f = %f ... delay current: %u tgt: %u. ret: %ld\n ", 
  //  step_counts, step_size, step_counts * step_size, delay_current, step_target, ret);

  return ret;
}
#endif