#include "stepper.h"

void Stepper::dprintf(uint8_t level, const char *format, ...) {
  if (level > debug_level) {
    return;
  }

  char buf[1024];
  va_list args;
  va_start(args, format);
  vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);

  Serial.write(buf);
}

void Stepper::choose_next() {
  choose_next(state_next);
}

void Stepper::choose_next(STEP_STATE next) {
  state = next;
  
  switch (state) {
    case STEP_INIT:
      if (DEFAULT_MODE_NEXT == STEP_SWEEP)
        state_next = STEP_SWEEP;
      else
        state_next = STEP_CLOSE;

      dprintf(LOG_DEBUG, "%d: Initializing\n", idx);
      set_target(-INT_MAX);
      accel.accel_0 = 0.00015;
      break;
    case STEP_WIGGLE_START:
      state_next = STEP_WIGGLE_END;
      dprintf(LOG_DEBUG, "%d: Wiggle start\n", idx);
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

      dprintf(LOG_DEBUG, "%d: Wiggle end. Next: %d\n", idx, state_next);
      set_onoff(STEPPER_OFF);
      choose_next_wiggle();
      break;
    case STEP_RANDOM_WALK:
      dprintf(LOG_DEBUG, "%d: Random walk start\n", idx);
      choose_next_rand_walk();
      break;
    case STEP_CLOSE:
      state_next = STEP_OPEN;
      set_target(pos_end);
      accel.set_pause_ms(10);
      dprintf(LOG_DEBUG, "%d: Doing close\n", idx);
      break;
    case STEP_OPEN:
      set_target(-INT_MAX);  // Force us to find the lower limit
      set_onoff(STEPPER_OFF);
      accel.set_pause_ms(250);
      dprintf(LOG_DEBUG, "%d: Doing open\n", idx);
      state_next = STEP_RELAX;
      break;
    case STEP_RELAX:
      state_next = STEP_WIGGLE_START;
      set_onoff(STEPPER_OFF);
      accel.set_pause_ms(2500);
      dprintf(LOG_DEBUG, "%d: Doing relax\n", idx);
      break;
    case STEP_TRIGGERED_INIT:
      dprintf(LOG_DEBUG, "%d: Doing triggered init\n", idx);
      // Start by opening a little 
      set_target(position * 0.9, settings_on_close);
      state_next = STEP_GRAB;
      break;
    case STEP_GRAB:
      set_target(pos_end, settings_on_close);
      accel.set_pause_ms(settings_on_close.pause_ms);
      dprintf(LOG_DEBUG, "%d: Doing grab\n", idx);
      state_next = STEP_GRAB_WIGGLE;
      break;
    case STEP_GRAB_WIGGLE:
      choose_next_wiggle(pos_end * .85, pos_end * .99);
      accel.set_pause_ms(random(10, 150));
      dprintf(LOG_DEBUG, "%d: Doing grab wiggle\n", idx);

      if(--how_wiggly > 0) {
        state_next = STEP_GRAB_WIGGLE;
      } else {
        state_next = STEP_OPEN;
      }
      break;
    case STEP_DETANGLE:
      dprintf(LOG_DEBUG, "%d: Doing detangle\n", idx);
      set_onoff(STEPPER_OFF);
      accel.set_pause_ms(random(100, 500));

      // XXX Revisit
      // Hack to reset us off -pos_end/4
      position = 0;
      // XXX switch to using time
      set_target(pos_end/8 * detangling, settings_on_open);
      state_next = STEP_OPEN;
      break;
    case STEP_SWEEP:
      choose_next_sweep();
      dprintf(LOG_DEBUG, "%d: Doing sweep\n", idx);
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
  accel.set_pause_ms(random(250, 1500));

  set_onoff(STEPPER_OFF);
  uint32_t r = random(lower, upper);
  int32_t nxt = 0;
  if (position != 0 && random() & 1)
    nxt = position - r;
  else
    nxt = position + r;

  nxt = min(nxt, upper);
  nxt = max(lower, nxt);

  set_target(nxt);

  if(nxt != position)
    dprintf(LOG_DEBUG, "%d: wiggle next: from %ld to %ld\n", idx, position, nxt);
}

void Stepper::choose_next_sweep() {
  accel.set_pause_ms(1000);

  if (position == pos_end) {
    dprintf(LOG_DEBUG, "%d: At end. Reversing\n", idx);
    set_target(-INT_MAX);
  } else {
    set_target(pos_end);
    dprintf(LOG_DEBUG, "%d: At start. Cooling off\n", idx);
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
    accel.set_pause_min(); //enforce short pause before reversing

  forward = f;
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

  if(tgt == -INT_MAX)
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

  if (debug_level >= LOG_DEBUG && now - last_log > 1000) {
    uint32_t us = micros();
    last_log = now;

    dprintf(LOG_DEBUG, "%d: Position: %ld, Target: %ld, State: %d, Fwd/Back: %d, "
        "Accel Delay: %ld, A0: %f, is ready: %d !(%lu && %lu)\n",
      idx, position, pos_tgt == -INT_MAX ? -99999 : pos_tgt,
      state, forward, 
      accel.delay_current, accel.accel_0, accel.is_ready(),
      us - accel.t_last_update < accel.t_pause_for,
      us - accel.t_last_update < accel.delay_current);
  }

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
      dprintf(LOG_DEBUG, "%d: At low limit\n", idx);
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

    // This is to help with detangling
    if(state == STEP_OPEN && position < -pos_end/4) {
      detangling++;
      // XXX this doesn't work. We'll try to close to pos_end*.85 then open again,
      // but even if detangled, we can hit -pos_end/4 because we lost steps
      // Need to use time instead.
      // See position hack in step_detangle case
      choose_next(STEP_DETANGLE);
      if(detangling > MAX_DETANGLE_TRIES) {
        detangling = MAX_DETANGLE_TRIES;
        dprintf(LOG_DEBUG, "%d: Failing to detangle :/\n", idx);
      }
      return;
    } 
    else {
      detangling = 0;

    }

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
  switch(state) {
    case STEP_INIT:
    case STEP_TRIGGERED_INIT:
    case STEP_GRAB:
    case STEP_GRAB_WIGGLE:
    case STEP_CLOSE:
      return;
  }

  how_wiggly = random(2, 4);
  choose_next(STEP_TRIGGERED_INIT);
}