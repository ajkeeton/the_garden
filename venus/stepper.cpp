#include "stepper.h"

void Stepper::choose_next() {
  switch(state_next) {
    case STEP_FIND_ENDPOINT_1:
      state = STEP_FIND_ENDPOINT_1;
      state_next = STEP_FIND_ENDPOINT_2;

      // XXX Add random pause and delay
      if(forward)
        set_target(INT_MAX);
      else 
        set_target(-INT_MAX);

        randomize_delay();
      break;
    case STEP_FIND_ENDPOINT_2:
      state = STEP_FIND_ENDPOINT_2;
      state_next = STEP_DEFAULT;

      // XXX Add random pause and delay
      if(forward) {
        set_target(-INT_MAX);
      }
      else {
        set_target(INT_MAX);
      }

      randomize_delay();
      break;
    case STEP_WIGGLE_START:
      state = STEP_WIGGLE_START;
      state_next = STEP_WIGGLE_END;
      break;
    case STEP_WIGGLE_END:
      state = STEP_WIGGLE_END;
      state_next = STEP_DEFAULT;
      break;
    case STEP_RANDOM_WALK:
      state = STEP_RANDOM_WALK;
      choose_next_rand_walk();
      break;
    case STEP_SWEEP:
      Serial.printf("Doing sweep - %u %u\n", tpause, t_pulse_delay);
      state = STEP_SWEEP;
      if(forward) {
        set_target(pos_start);
      } 
      else {
        set_target(pos_end);
      }
      break;
    default:
      break;
  };
}

void Stepper::choose_next_rand_walk() {
}

void Stepper::choose_next_sweep() {
  cur_num_steps = 0;
  tpause = 1000; 
  tpause *= 1000; // to micros
  t_pulse_delay = delay_max;

  if(position == pos_end) {
    set_target(pos_start);
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

void Stepper::randomize_delay() {
  tpause = random(100, 1000); 
  tpause *= 1000; // to micros

  t_pulse_delay = random(delay_min, delay_max);
}

void Stepper::set_forward(bool f) {
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

void Stepper::set_target(int32_t tgt, uint32_t tp, uint32_t td) {
  if(tp)
    t_pulse_delay = tp;
  if(td)
    t_pulse_delay = td;

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

void Stepper::run() {
  uint32_t now = micros();

  if(now < tlast + t_pulse_delay + tpause) 
    return;

  tpause = 0;
  tlast = now;

  set_onoff(STEPPER_ON);

  // XXX Need to handle case we were already triggered
  if(false && check_limit()) {
    if(!at_limit) {
      if(forward) {
        // Handle case we're reversed
        if(pos_end < 0)
          pos_end = position + 1;
        else
          pos_end = position - 1;

        Serial.printf("At limit, new end: %u\n", pos_end);
      }
      else {
        // Handle case we're reversed
        if(pos_start > pos_end)
          pos_start = position - 1;
        else
          pos_start = position + 1;

        Serial.printf("At limit, new start: %u\n", pos_start);
      }

      at_limit = true;
    }
  }
  else
    at_limit = false;

  if(forward)
    position++;
  else
    position--;

  digitalWrite(pin_step, step_pin_val);
  step_pin_val = !step_pin_val;
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
    t_pulse_delay--;
  }
  // decel
  else
    //tdelay = tdelay * (4 * cur_num_steps + 1) / (4 * cur_num_steps - 1);
    //tdelay = tdelay * (4 * cur_num_steps + 1) / (4 * cur_num_steps);
    t_pulse_delay++;
  #endif

  //Serial.printf("raw: %d\n", tdelay);

  if(t_pulse_delay < delay_min)
    t_pulse_delay = delay_min;
  if(t_pulse_delay > delay_max)
    t_pulse_delay = delay_max;

  // Serial.printf("position: %ld vs %ld\n", position, pos_tgt);
  
  if(position != pos_tgt)
    return;

  Serial.printf("At target! Pos: %d Num steps: %d\n", position, cur_num_steps);

  choose_next();

  //delay(2000);
}
