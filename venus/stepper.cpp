#include "stepper.h"

void Stepper::run() {
  uint32_t now = micros();

  if(now < tlast + tdelay + tpause) 
    return;

  tpause = 0;
  tlast = now;

  set_onoff(true);

  if(forward)
    position++;
  else
    position--;

  digitalWrite(pin_step, state_step);
  state_step = !state_step;
  cur_num_steps++;

  //if(!(cur_num_steps % 2500))
  //  Serial.printf("delay: %u, position: %d, fwd/back: %d, num_step: %d, tgt: %d, decel @: %d\n", 
  ///    tdelay, position, forward, cur_num_steps, pos_tgt, pos_decel);

  //if(position > pos_end)
  //  pos_end = position; // Once we hit the end limit switch this will be correct

  #if 0
  // https://www.littlechip.co.nz/blog/a-simple-stepper-motor-control-algorithm
  // accel
  if(cur_num_steps < pos_decel) {
    //Serial.print("accel - ");
    //tdelay = tdelay * (4 * cur_num_steps - 1) / (4 * cur_num_steps + 1);
    //tdelay = tdelay * (4 * cur_num_steps) / (4 * cur_num_steps + 1);
    tdelay--;
  }
  // decel
  else
    //tdelay = tdelay * (4 * cur_num_steps + 1) / (4 * cur_num_steps - 1);
    //tdelay = tdelay * (4 * cur_num_steps + 1) / (4 * cur_num_steps);
    tdelay++;
  #endif

  //Serial.printf("raw: %d\n", tdelay);

  if(tdelay < delay_min)
    tdelay = delay_min;
  if(tdelay > delay_max)
    tdelay = delay_max;

  //Serial.printf("position: %ld vs %ld\n", position, pos_tgt);

  if(position != pos_tgt)
    return;

  Serial.printf("At target! Pos: %d Num steps: %d\n", position, cur_num_steps);

  choose_next();

  //delay(5000);
}
