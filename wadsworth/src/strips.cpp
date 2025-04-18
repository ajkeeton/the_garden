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

  layer_glow.init(nleds, true);
  layer_waves.init(nleds, false);
  layer_tracers.init(nleds, false);
  transition.init(nleds, false);
  
  rainbow.init(nleds);
  waves.init(layer_waves.targets, nleds);
  wave_pulse.init(layer_waves.targets, nleds);
  blobs.init(layer_waves.targets, nleds);
  blob_asc.init(layer_waves.targets, nleds);
  white.init(transition.targets, nleds);

  /*
  if(tracers_sens_map) {
    // If tracers_sens_map is already allocated, delete it
    delete []tracers_sens_map;
  }
  tracers_sens_map = new uint8_t[num_leds];
  for(int i=0; i<num_leds; i++) {
    tracers_sens_map[i] = 0; // Point all LEDs to the first sensor tracer for now
  }
  */

  tracer_sens.init(layer_tracers.targets, nleds, 0);

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

bool strip_t::on_trigger(uint16_t led, uint16_t percent, uint32_t duration) {
  // Trigger a steady glow and a pulse at the sensor
  uint16_t spread = map(percent, 0, 100, 0, 60);//num_leds/10);

  for(int i=0; i<spread; i++) {
    int j = led - i;
    int k = led + i;
    uint8_t brightness = 255 - map(i, 0, spread, 0, 255);

    if(j >= 0) {
      CRGB color = rainbow.get(j, brightness);
      layer_glow.set(j, color, 250);
    }
    if(k < num_leds) {
      CRGB color = rainbow.get(k, brightness);
      layer_glow.set(j, color, 250);
    }
  }

  layer_glow.blur(160);

  // Return value determines if we're going to signal the garden
  return tracer_sens.trigger(led, percent, duration);
  //uint8_t tidx = tracers_sens_map[led];
  //tracers_sens[tidx].trigger(percent, duration);
}

void strip_t::step(uint32_t global_ac) {
  uint32_t now = millis();
  uint32_t lapsed = now - last_update;

  if(lapsed < 2)
    return;

  last_update = now;

  // handle_low_activity();
  // handle_low_power(); 
  // handle_global_pulse();

  for(int i=0; i<num_leds; i++) {
    #warning figure out best to additively blend layers
      layer_glow.blend(leds[i], i, 255);
      layer_waves.blend(leds[i], i, 20);
      transition.blend(leds[i], i, 80);
      layer_tracers.blend(leds[i], i, 80);
    //}
    //leds[i] = layer_tracers.leds[i];
    //nblend(leds[i], layer_tracers.leds[i], 255);
  }
  
  layer_glow.fade(5);

  //Serial.println();
}

void strip_t::background_update(uint16_t global_activity) {
  // XXX Future: Only update when in a state a pattern that might use it
  rainbow.update();

  tracer_sens.step();

  switch(pattern) {
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


bool strip_t::near_mids(int lidx) {
#if 0
  for(int i=0; i<nsens; i++)
    if(dist(lidx, sensors[i].location) < 5)
      return true;
#endif

return false;
}

void strip_t::do_basic_ripples(uint16_t activity) {
  for(int i=0; i<n_rand_tracers; i++)
    tracers_rand[i].step(activity);

  // high activity increases fade
  // 0 -> 3
  // 100 -> 20
  uint16_t fade = map(activity, 0, 100, 10, 30);
  layer_tracers.fade(fade);
}

void strip_t::do_high_energy_ripples(uint16_t activity) {
  for(int i=0; i<n_rand_tracers; i++)
    tracers_rand[i].step(activity);

  uint16_t fade = map(activity, 0, 100, 10, 30);
  layer_tracers.fade(fade);
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


void strip_t::go_white() {
  #if 0
  for(int i=0; i<num_leds; i++) {
    targets[i] = CRGB::White; 
    lifespans[i] = 750;

    // Hack for better transitions
    layer_tracers[i] = layer_waves[i] = CRGB::Black;
  }
  #endif
}

void strip_t::handle_pulse() {
#if 0 
  if(!mstate.pulse.is_triggered())
    return;

  for(int i=0; i<num_leds; i++) {
    targets[i] = CRGB::White; 
    lifespans[i] = 200;

    // Hack for better transitions
    layer_tracers[i] = layer_waves[i] = CRGB::White;
  }

  if(!mstate.pulse.do_once)
    return;
  
  mstate.pattern++;
  mstate.pulse.do_once = false;
#endif
}

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
