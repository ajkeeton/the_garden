#include "stepper.h"

void Stepper::choose_next() {
  state = state_next;

  switch (state) {
    case STEP_INIT:
      if (DEFAULT_MODE_NEXT == STEP_SWEEP)
        state_next = STEP_SWEEP;
      else
        state_next = STEP_CLOSE;

#ifdef DEBUG_STEP
      Serial.printf("%d: Initializing\n", idx);
#endif
      set_target(-INT_MAX);
      accel.accel_0 = 0.00015;
      break;
#if 0
    case STEP_FIND_ENDPOINT:
      state_next = STEP_WIGGLE_START;

      // XXX Add random pause and delay
      if(forward)
        set_target(pos_end);
      else 
        set_target(-INT_MAX);

        randomize_delay();
      break;
#endif
    case STEP_WIGGLE_START:
      state_next = STEP_WIGGLE_END;
      choose_next_wiggle();
      break;
    case STEP_WIGGLE_END:
      //if(!(random() % 6))
      //  state_next = STEP_CLOSE;
      if (!(random() % 10))
        state_next = STEP_OPEN;
      else {
        state_next = STEP_WIGGLE_START;
      }

      set_onoff(STEPPER_OFF);
      choose_next_wiggle();
      break;
    case STEP_RANDOM_WALK:
      choose_next_rand_walk();
      break;
    case STEP_CLOSE:
      state_next = STEP_OPEN;
      set_target(pos_end);

      accel.set_pause_ms(10);
#ifdef DEBUG_STEP
      Serial.printf("%d: Doing close\n", idx);
#endif
      break;
    case STEP_OPEN:
      set_target(-INT_MAX);  // Force us to find the lower limit
      set_onoff(STEPPER_OFF);
      accel.set_pause_ms(100);

#ifdef DEBUG_STEP
      Serial.printf("%d: Doing open\n", idx);
#endif
      state_next = STEP_RELAX;
      break;
    case STEP_RELAX:
      state_next = STEP_WIGGLE_START;
      set_onoff(STEPPER_OFF);
      accel.set_pause_ms(2500);
#ifdef DEBUG_STEP
      Serial.printf("%d: Doing relax\n", idx);
#endif
      break;
    case STEP_TRIGGERED_INIT:
#ifdef DEBUG_STEP
      Serial.printf("%d: trigger init - opening\n", idx);
#endif

      if (position > 1000)
        set_target(-INT_MAX, settings_on_close);
      state_next = STEP_GRAB;
      break;
    case STEP_GRAB:
      set_target(pos_end, settings_on_close);
      accel.set_pause_ms(settings_on_close.pause_ms);
#ifdef DEBUG_STEP
      Serial.printf("%d: Doing grab\n", idx);
#endif
      state_next = STEP_GRAB_WIGGLE;
      break;
    case STEP_GRAB_WIGGLE:
      choose_next_wiggle(pos_end * .85, pos_end * .98);
      accel.set_pause_ms(random(10, 50));

#ifdef DEBUG_STEP
      Serial.printf("%d: Doing grab wiggle\n", idx);
#endif
      state_next = STEP_OPEN;
      /*
      if(!(random()%5)) {
          set_target(-INT_MAX, settings_on_wiggle);
          state_next = STEP_OPEN;
#ifdef DEBUG_STEP
          Serial.printf("%d: time to open again\n", idx);
#endif
      }
      else
          state_next = STEP_GRAB;
      */

      break;
    case STEP_SWEEP:
      choose_next_sweep();
      break;
    default:
      break;
  };
}

void Stepper::choose_next_rand_walk() {
}

void Stepper::choose_next_wiggle() {
  choose_next_wiggle(100, settings_on_wiggle.max_pos);
}

void Stepper::choose_next_wiggle(int32_t lower, int32_t upper) {
  accel.set_pause_ms(random(10, 500));
  //if(!forward)
  set_onoff(STEPPER_OFF);

  uint32_t r = random(lower, upper);
  int32_t nxt = 0;
  if (position != 0 && random() & 1)
    nxt = position - r;
  else
    nxt = position + r;

  nxt = min(nxt, upper);
  nxt = max(lower, nxt);

#ifdef DEBUG_STEP
  Serial.printf("%d: wiggle next: from %ld to %ld\n", idx, position, nxt);
#endif

  set_target(nxt);
}

void Stepper::choose_next_sweep() {
  accel.set_pause_ms(1000);

  if (position == pos_end) {
#ifdef DEBUG_STEP
    Serial.printf("%d: At end. Reversing\n", idx);
#endif
    set_target(-INT_MAX);
  } else {
    set_target(pos_end);
#ifdef DEBUG_STEP
    Serial.printf("%d: At start, cooling off\n", idx);
#endif
    set_onoff(STEPPER_OFF);
  }

  accel.accel_0 = 0.00002;
  accel.delay_min = DELAY_MIN;
}

void Stepper::randomize_delay() {
  accel.set_pause_ms(random(100, 1000));
  accel.delay_current = random(pulse_delay_min, pulse_delay_max);
}

void Stepper::set_forward(bool f) {
  if (forward != f)
    accel.set_pause_min();  //enforce short pause before reversing

  forward = f;
  // Serial.printf("direction: %d\n", forward);
  if (forward) {
    digitalWrite(pin_dir, val_forward);
  } else {
    digitalWrite(pin_dir, val_backward);
  }
}

void Stepper::set_target(int32_t tgt) {
  set_target(tgt, settings_on_wiggle);  // XXX use a default setting?
}

void Stepper::set_target(int32_t tgt, const step_settings_t &ss) {
  pos_tgt = tgt;

  if (tgt == -INT_MAX)
    tgt = 0;

#if 0
  uint32_t dmin = DELAY_MIN;
  float transition_at = 0.4;
  if(state == STEP_WIGGLE_START || state == STEP_WIGGLE_END) {
    dmin = 150;
    transition_at = .4;
  }

  accel.set_step_size(tgt, position, transition_at, dmin);
#endif

// Args:
//  - Scale by number of steps to the target full speed or full stop
//  - Max delay
//  - Min delay
//  - A constant to scale the calc
#if 0
  if(state == STEP_WIGGLE_START || state == STEP_WIGGLE_END)
    accel.set_target(abs(tgt - position)*.5, DELAY_MAX, 750, 0.0000025);
  else
    accel.set_target(abs(tgt - position)*.5, DELAY_MAX/2, 100, 0.000005);
#endif

  uint32_t steps_to_mid_point = abs(tgt - position) * .5;
  uint32_t maxd = ss.max_delay,
           mind = ss.min_delay;
  if (pos_tgt == -INT_MAX) {
    steps_to_mid_point = position / 2;
    maxd = 500;
    mind = 100;
  }

  accel.set_target(steps_to_mid_point, maxd, mind, ss.accel);

  if (pos_tgt > position) {
    set_forward(true);
  } else if (pos_tgt < position) {
    set_forward(false);
  }

  //Serial.printf("%d: Position: %d, New target: %d End: %d fwd/back: %d\n",
  //  idx, position, pos_tgt == -INT_MAX ? 0 : pos_tgt, pos_end, forward);
}

void Stepper::run() {
  uint32_t now = millis();

#ifdef DEBUG_STEP
  if (now - last_log > 1000) {
    uint32_t us = micros();
    Serial.printf("%d: Position: %ld, Target: %ld, State: %d, Fwd/Back: %d, Accel Delay: %ld, A0: %f, is ready: %d (%lu < %lu, %d)\n",
      idx, position, pos_tgt == -INT_MAX ? -99999 : pos_tgt, 
      state, forward, accel.delay_current, accel.accel_0, accel.is_ready(), 
      us, accel.t_pause_for, us - accel.t_last_update < accel.delay_current);
    last_log = now;
  }
#endif

  if (!accel.is_ready())
    return;

  set_onoff(STEPPER_ON);

  TRIGGER_STAT ls = TRIGGER_OFF;
#ifdef NO_LIMIT_SWITCH
  if (!position)
    ls = TRIGGER_ON;
#else
  ls = limits.check_triggered();
#endif

  switch (ls) {
    case TRIGGER_ON:
#ifdef DEBUG_STEP
      Serial.printf("%d: At low limit\n", idx);
#endif
      position = 0;
      set_target(10);
      break;
    case TRIGGER_WAIT:
      // We may technically change modes and back into the LS again
      // while triggered
      // Force us to continue moving away from switch even though triggered
      if (!forward) {
        // Force us to choose a new direction/mode
        position = 0;
        set_target(10);
      }
      break;
    default:
      break;
  }

  if (position != pos_tgt) {
    if (forward)
      position++;
    else
      position--;

    digitalWrite(pin_step, step_pin_val);
    step_pin_val = !step_pin_val;
    accel.next_plat();
  }

  //if(!(position % 100) || position == pos_tgt)
  //  Serial.printf("%d: Position: %ld, Target: %ld (pulse delay: %u, pos_end: %ld)\n",
  //    idx, position, pos_tgt == -INT_MAX ? 0 : pos_tgt, accel.delay_current, pos_end);

  if (position == pos_tgt)
    choose_next();
}

void Stepper::trigger_close() {
  if (state == STEP_INIT || state == STEP_TRIGGERED_INIT)
    return;

  //if(state != STEP_WIGGLE_START && state != STEP_WIGGLE_END)
  //  return;

  uint32_t now = millis();

  //if(now - last_close < 1000)
  //  return;

  last_close = now;

#ifdef DEBUG_STEP
  Serial.printf("%d: Triggered\n", idx);
#endif

  state_next = STEP_TRIGGERED_INIT;
  choose_next();
}