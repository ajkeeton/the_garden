#include "stepper.h"

void Stepper::choose_next() {
  switch(state_next) {
    case STEP_FIND_ENDPOINT_1:
      state = STEP_FIND_ENDPOINT_1;
      state_next = STEP_FIND_ENDPOINT_2;

      // XXX Add random pause and delay
      if(forward)
        set_target(pos_end);
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
        set_target(pos_end);
      }

      randomize_delay();
      break;
    case STEP_WIGGLE_START:
      state = STEP_WIGGLE_START;
      state_next = STEP_WIGGLE_END;
      choose_next_wiggle();
      break;
    case STEP_WIGGLE_END:
      state = STEP_WIGGLE_END;
      /*
      if(!(random() % 6))
        state_next = STEP_CLOSE;
      else if (!(random() % 3))
        state_next = STEP_OPEN;
      else {
      */
        state_next = STEP_WIGGLE_START;
        set_onoff(STEPPER_OFF);
        choose_next_wiggle();
      //}
      break;
    case STEP_RANDOM_WALK:
      state = STEP_RANDOM_WALK;
      choose_next_rand_walk();
      break;
    case STEP_CLOSE:
      state = STEP_CLOSE;
      state_next = STEP_OPEN;
      set_target(pos_end);
      //set_target(INT_MAX);  // Force us to the upper limit

      tpause = 50 * 1000;
      Serial.printf("Doing close - %u %u\n", tpause, t_pulse_delay);
      break;
    case STEP_OPEN:
      state = STEP_OPEN;
      set_target(-INT_MAX); // Force us to find the lower limit

      tpause = 3000 * 1000;
      //t_pulse_delay = pulse_delay_min;
      set_onoff(STEPPER_OFF);
      Serial.printf("%d: Doing open\n", idx);

      //state_next = STEP_CLOSE;
      state_next = STEP_RELAX;
      break;
    case STEP_RELAX:
      state_next = STEP_WIGGLE_START;
      tpause = 2500 * 1000;
      set_onoff(STEPPER_OFF);
      Serial.printf("%d: Doing relax\n", idx);
      break;
    case STEP_SWEEP:
      choose_next_sweep();
      Serial.printf("%d: Doing sweep\n", idx);
      break;
    default:
      break;
  };
}

void Stepper::choose_next_rand_walk() {
}

void Stepper::choose_next_wiggle() {
  tpause = random(500, 1500); 
  tpause *= 1000; // to micros

  int32_t nxt = 0;
  if(position != 0 && random()&1)
    nxt = position - random(100, 1000);
  else 
    nxt = position + random(100, 1000);

  Serial.printf("%d: wiggle next: %ld -> %ld\n", idx, position, nxt);

  nxt = min(nxt, pos_end/4);
  nxt = max(0, nxt);

  set_target(nxt);
  t_pulse_delay = pulse_delay_max;
}

void Stepper::choose_next_sweep() {
  tpause = 1000; 
  tpause *= 1000; // to micros
  t_pulse_delay = pulse_delay_max;

  if(position == pos_end) {
    Serial.printf("%d: At end. Reversing\n", idx);
    set_target(-INT_MAX);
    //delay(1000);
  }
  else {
    set_target(pos_end);
    Serial.printf("%d: At start, cooling off\n", idx);
    set_onoff(STEPPER_OFF);
    //delay(1000);
  }

  tlast = micros();
}

void Stepper::randomize_delay() {
  tpause = random(100, 1000); 
  tpause *= 1000; // to micros

  t_pulse_delay = random(pulse_delay_min, pulse_delay_max);
}

void Stepper::set_forward(bool f) {
  if(forward != f)
    tpause = max(20000, tpause); // min 20 ms before reversing

  forward = f;
  // Serial.printf("direction: %d\n", forward);
  if(forward) {
    digitalWrite(pin_dir, val_forward);
  }
  else {
    digitalWrite(pin_dir, val_backward);
  }
}

void Stepper::set_target(int32_t tgt) {
  #ifdef NO_LIMIT_SWITCH
  if(tgt == -INT_MAX)
    tgt = 0;
  #endif

  t_pulse_delay = accel.delay_max;
  accel.set_target(8, 2);

  pos_tgt = tgt;

  if(pos_tgt > position) {
    // closing / grabbing
    pos_to_decel = (pos_tgt - position) * .8;

    // XXX experimenting
    if(state == STEP_CLOSE) { 
      accel.set_target(16, 10);
      accel.delay_current /= 4; 
      t_pulse_delay /= 4;
    }
    // XXX

    set_forward(true);

  }
  else if (pos_tgt < position) {
    // opening
    if(tgt == -INT_MAX)
      pos_to_decel = position * .4;
    else
      pos_to_decel = (position - pos_tgt) * .4;

    set_forward(false);
  }
  else {
    // Position and target are the same

    //if(pos_tgt == pos_end) {
      set_forward(false); // Hack since we always take one step before checking
    //}
  }

  Serial.printf("%d: Position: %d, New target: %d End: %d fwd/back: %d\n", 
    idx, position, pos_tgt == -INT_MAX ? 0 : pos_tgt, pos_end, forward);
}

void Stepper::run() {
  uint32_t now = micros();

  if(now < tlast + t_pulse_delay + tpause) 
    return;

  tpause = 0;
  tlast = now;

  set_onoff(STEPPER_ON);
  
  switch(limits.check_triggered()) {
    case TRIGGER_ON:
        if(limits.triggered_high) {
          // if(!forward) // set reversed
          position--;
          pos_tgt = pos_end = position;
          Serial.printf("At high limit, new end: %u\n", pos_end);
        }
        else {
          // if(forward) // set reversed
          pos_tgt = position = 0;
          Serial.printf("At low limit\n");
        }
        break;
    case TRIGGER_WAIT:
        // Continue moving in our current direction even though triggered
        if((position == pos_end) && forward || !position && !forward) {
          pos_tgt = position;
          break;
        }
    default:
        if(forward)
          position++;
        else
          position--;

        digitalWrite(pin_step, step_pin_val);
        step_pin_val = !step_pin_val;
        t_pulse_delay = accel.next();
      break;
  }

  if(!(position % 200) || position == pos_tgt)
    Serial.printf("%d: position: %ld. Target: %ld (pulse delay: %u, decel @ %ld)\n", 
      idx, position, pos_tgt, t_pulse_delay, pos_to_decel);
  
  if(position == pos_to_decel)
    accel.set_decel(t_pulse_delay);

  if(position != pos_tgt)
    return;

  Serial.printf("%d: At target! Pos: %d\n", idx, position);

  choose_next();
}
