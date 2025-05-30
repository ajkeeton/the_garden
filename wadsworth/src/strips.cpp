#include "common.h"
#include "strips.h"
#include "state.h"

#if 0
int _dist(uint8_t l, uint8_t r) {
  int d = (int)l - (int)r;
  return d*d;
}

int pdist(CRGB *l, CRGB *r) {
  return sqrt(_dist(l->r, r->r) + _dist(l->g, r->g) + _dist(l->b, r->b));
}
#endif

int dist(uint8_t i, uint8_t j) {
  if(i < j)
    return j - i;
  return i - j;
}

void strip_t::init(CRGB *l, uint16_t nleds, bool is_ctr) {
  is_center = is_ctr;
  leds = l;
  num_leds = nleds;

  for(int i=0; i<num_leds; i++) {
    leds[i] = CRGB::Black;
  }

  layer_white.init(nleds, false);
  layer_colored_glow.init(nleds, true);
  layer_waves.init(nleds, false);
  layer_tracers.init(nleds, false);
  layer_transition.init(nleds, false);
  
  // These are the LED animations themselves
  rainbow.init(nleds);
  waves.init(layer_waves.targets, nleds);
  wave_pulse.init(layer_waves.targets, nleds);
  blobs.init(layer_waves.targets, nleds);
  blob_asc.init(layer_waves.targets, nleds);
  // white.init(layer_white.targets, nleds);

  if(triggered) {
    delete []triggered;
  }
  triggered = new bool[num_leds];

  for(int i=0; i<num_leds; i++) {
    triggered[i] = false;
  }

  tracer_sens.init(layer_tracers.targets, nleds);
  reverse_pulse.init(layer_tracers.targets, nleds);

  #if 0
  if(tracers_sens)
    delete []tracers_sens;
  tracers_sens = new tracer_t[num_leds];
  for(int i=0; i<num_leds; i++) {
    // XXX Technically only need one of these for each sensor, at the respective LED, 
    // but doing it this way for now
    tracers_sens[i].init(layer_tracers.targets, nleds, i);
  }
  #endif
  for(int i=0; i<n_rand_tracers; i++) {
    tracers_rand[i].init(layer_tracers.targets, nleds, 0);
  }
}

void strip_t::on_trigger(uint16_t led, uint16_t percent, uint32_t duration) {
  // Trigger a steady glow and a pulse at the sensor
  uint16_t spread = map(percent, 0, 100, 0, 60);//num_leds/10);

  for(int i=0; i<spread; i++) {
    int j = led - i;
    int k = led + i;
    uint8_t brightness = 255 - map(i, 0, spread, 0, 255);

    if(j >= 0) {
      CRGB color = rainbow.get(j, brightness);
      layer_colored_glow.set(j, color, 250);
    }
    if(k < num_leds) {
      CRGB color = rainbow.get(k, brightness);
      layer_colored_glow.set(k, color, 250);
    }
  }

  layer_colored_glow.blur(160);

  // Serial.printf("strips::on_trigger pct: %u, duration: %u\n", percent, duration);
  if(percent > 60) {
    if(!triggered[led]) {
      tracer_sens.trigger(led, percent - 59, duration);

      // If this is one of the strips that is bent in half, launch a second 
      // tracer in reverse to have full coverage
      if(trigger_both_directions) {
        // Setting t_last to 0 is a hack to make having two of these going in 
        // opposite directions work, otherwise it would be throttled and ignored
        tracer_sens.t_last = 0;
        tracer_sens.trigger(led, percent - 59, duration, true);
      }

      // Make sure we can only have this tracer going once
      triggered[led] = true;
    }
  }
}

void strip_t::on_trigger_cont(uint16_t led, uint16_t percent, uint32_t duration) {
  on_trigger(led, percent, duration);
}

void strip_t::on_trigger_off(uint16_t led, uint16_t percent, uint32_t duration) {
  triggered[led] = false; 
}

void strip_t::step(uint32_t global_ac) {
  uint32_t now = millis();
  uint32_t lapsed = now - t_last_update;

  if(lapsed < 1)
    return;

  t_last_update = now;

  for(int i=0; i<num_leds; i++) {
    #warning figure out best to additively blend layers
      layer_colored_glow.blend(leds[i], i, 255);
      layer_waves.blend(leds[i], i, 20);
      //transition.blend(leds[i], i, 80);
      layer_tracers.blend(leds[i], i, 80);
    //}
    //leds[i] = layer_tracers.leds[i];
    //nblend(leds[i], layer_tracers.leds[i], 255);
  }
  
  layer_colored_glow.fade(5);
}

void strip_t::fade_all(uint16_t amount) {
  // XXX amount is a percentage with an extra 0
  // The fade amount is out of 255, and we update fast enough that we want to 
  // scale that back
  amount = map(amount, 0, 1000, 0, 20);

  for(int i=0; i<num_leds; i++) {
    layer_colored_glow.fade(amount);
    layer_waves.fade(amount);
    layer_tracers.fade(amount);
  }
}

// Update the layers, but don't push to the LEDs
void strip_t::background_update(meta_state_t &state) {
  uint16_t amount = 0;
  if((amount = state.sleep.amount())) {
    fade_all(amount);
    return;
  }

  if((amount = state.pulse_white.amount())) {
    pulse_white.step(amount);
  }

  if((amount = state.ohai.amount())) {
    // Use the current rainbow color to apply a glow. TODO: make it blobs
    uint16_t b = map(amount, 0, 1000, 10, 255);
    CRGB color = rainbow.get(0, b);
    for(int i=0; i<num_leds; i++) {
      layer_colored_glow.set(i, color, 255);
    }
  }

  if((amount = state.pending_transition.amount())) {
    // XXX Future: add a pre-transition mode
    // About to transition... reflected in the tracer motion?
  }

  uint16_t global_activity = state.percent_active();
  
  // XXX Future: Only update when in a state a pattern that might use it
  // if(need_rainbow...
  rainbow.update();
  tracer_sens.step();
  reverse_pulse.step();

  switch(state.pattern_idx) {
    case 0:
      //if(is_center)
        blobs.step(global_activity);
      //else // since not doing blobs, need to fade out any leftovers
      //  fadeToBlackBy(layer_waves, num_leds, 10);
      
      do_basic_ripples(global_activity);

      // do_high_energy_ripples(activity);
      break;
    case 1:
      if(is_center)  
        wave_pulse.step(global_activity);
      else {
        layer_waves.fade(10);
        //blobs.step(activity);
        //do_basic_ripples(activity);
        do_high_energy_ripples(global_activity);
      }
      break;
    case 2:
      //if(is_center) 
        //waves.step(beatsin8(30, 15, 200), 10, my_activity);
        blob_asc.step(global_activity);
      //else // since not doing blobs, need to fade out any leftovers
      //  fadeToBlackBy(layer_waves, num_leds, 10); 

      do_basic_ripples(global_activity);
      break;
    //  rainbow? 
    //  black ripples
    default:
      pattern = 0;
    }
}

void strip_t::do_basic_ripples(uint16_t activity) {
  for(int i=0; i<n_rand_tracers; i++)
    tracers_rand[i].step(activity);

  // high activity increases fade
  uint16_t fade = 10;
  //uint16_t fade = map(activity, 0, 100, 10, 30);
  layer_tracers.fade(fade);
}

void strip_t::do_high_energy_ripples(uint16_t activity) {
  for(int i=0; i<n_rand_tracers; i++)
    tracers_rand[i].step(activity);

  uint16_t fade = map(activity, 0, 100, 10, 30);
  layer_tracers.fade(fade);
}

bool strip_t::near_mids(int lidx) {
  #if 0
    for(int i=0; i<nsens; i++)
      if(dist(lidx, sensors[i].location) < 5)
        return true;
  #endif

  return false;
}

void strip_t::find_mids() {
  #if 0
  static uint32_t last = 0; 
  uint32_t now = millis();

  if(now - last > 250) {
    FastLED.clear();
    last = now;
    Serial.println("");
    return;
  }

  Serial.print(nsens);
  for(int i=0; i<nsens; i++) {
    Serial.print(" trigger ");
    Serial.print(i);
    Serial.print(" led ");
    Serial.print(sensors[i].location);
  }
  Serial.println();

  for(int i=0; i < nsens; i++) {
    leds[sensors[i].location] = CRGB::White;
  }

  blur1d(leds, num_leds, 128);
  #endif
}

// Force all LEDs white, skip the layers. Mostly for debugging
void strip_t::force_white() {
  for(int i=0; i<num_leds; i++) {
    leds[i] = CRGB::White;
  }
}

void strip_t::handle_remote_pulse(
    uint32_t color, uint8_t fade, uint16_t spread, uint32_t delay) {
    // Start a new tracer that moves up from the bottom  
    reverse_pulse.trigger(color, fade, spread, delay);
}

#if 0
void strip_t::handle_low_activity() {
//  if(mstate.active.is_triggered())
//    return;
}

void strip_t::handle_low_power() {
//  if(!mstate.low_power.is_triggered())
//    return;

  // Fade out
//  for(int i=0; i<num_leds; i++)
//    targets[i] = CRGB::Black;
}
#endif