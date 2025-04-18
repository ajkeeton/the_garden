#include "common.h"
#include "wadsworth.h"
#include "state.h"

bool pattern_state_t::can_trigger() {
  return true;
#if 0
  uint32_t now = millis();
  if(now - last < rest)
    return false;
  return true;
#endif
}

bool pattern_state_t::is_triggered() {
  bool was_active = active;

  if(!resting() && score > thold_act && !need_retrigger) {
    active = true;
  }

  if(score < thold_deact) {
    active = false;
    need_retrigger = false;
  }

  if(was_active) {
    if(active && max_duration) {
      if(millis() - t_activated > max_duration) {
        active = false;
        need_retrigger = true;
      }
    }

    if(!active) {
      if(rest)
        rest_until = millis() + rest;
    }
  }
  else {
    // wasn't active, but now is
    if(active) {
      t_activated = millis();
      do_once = true;
    }
  }

  return active;
}

bool pattern_state_t::resting() {
  if(active || !rest)
    return false;

  uint32_t now = millis();

  if(now < rest_until) {
    return true;
  }

  return false;
}

void pattern_state_t::update_avg() {
  avg = avg*(n_samples-1) + score;
  n_samples++;
  if(n_samples > N_STATE_SAMPLES)
    n_samples = N_STATE_SAMPLES;
  avg /= n_samples;
}

void pattern_state_t::trigger() {
  if(!max_score && score >= thold_act*2)
    return;

  if(score >= max_score)
    return;

  if(resting())
    return;

  score++;
  update_avg();
}

void pattern_state_t::untrigger() {
  if(score > 0)
    score--;

  update_avg();
}

#if 0
uint16_t meta_state_t::pulse_thold() {
  // Should be set based on number of sensors, and change according to commands
  // from the garden server
  // XXX
  return 1;
}

void meta_state_t::update(uint16_t ntrig, uint32_t total_score) {
  uint32_t now = millis();
  if(ntrig) {
    last_active = now;
    active.trigger();

    // If all are active, we're either by a poofer or in the sun - start phasing out until fewer are triggered
    //if(total >= ceil(num_sens * 0.9)) {
    if(ntrig >= pulse_thold()) {
      //low_power.trigger();
      //white.trigger();
      pulse.trigger();
    }
  }
  else {
    pulse.untrigger();
    active.untrigger();
  }
}
#endif