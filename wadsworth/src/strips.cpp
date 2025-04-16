#include "common.h"

extern meta_state_t mstate;

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
  nsens = 0;

  if(targets)
    delete []targets;
  targets = new CRGB[nleds];
  if(lifespans)
    delete []lifespans;
  lifespans = new uint16_t[nleds];

  if(layer_tracers)
    delete []layer_tracers;
  if(layer_waves)
    delete []layer_waves;
  if(transition)
    delete []transition;

  layer_tracers = new CRGB[nleds];
  layer_waves = new CRGB[nleds];
  transition = new CRGB[nleds];

  for(int i=0; i<nleds; i++) {
    leds[i] = targets[i] = CHSV(DEF_HUE, DEF_SAT, DEF_BRIGHT);
    layer_tracers[i] = CRGB::Black;
    layer_waves[i] = CRGB::Black;
    transition[i] = CRGB::Blue;
    lifespans[i] = 200;
  }

  waves.init(layer_waves, nleds);
  wave_pulse.init(layer_waves, nleds);
  rainbow.init(nleds);
  blobs.init(layer_waves, nleds);
  blob_asc.init(layer_waves, nleds);
  white.init(transition, nleds);

  for(int i=0; i<num_leds/50 && i < 4; i++) {
    tracers_rand[i].init(layer_tracers, num_leds, 0);
    n_tracers++;
  }

  last_update = millis();
}

// Each strip should "know" which sensors it needs to query and there LED offset
void strip_t::add_input(Mux_Read *mux, uint8_t mx_pin, uint16_t led) {
  tracers_sens[nsens].init(layer_tracers, num_leds, led);
  sensors[nsens++].init(mux, mx_pin, led);
  mstate.num_sens++;
}

void strip_t::update_triggered(uint32_t now, int i) {
  if(now - sensors[i].start > 20) { 
    // Wait a few ms to effectively debounce
    if(sensors[i].pending_pulse) {
      sensors[i].pending_pulse = false;
      tracers_sens[i].reset(250, /*250,*/ -1);
    }
  }

  // Scale by sensor score
  int n = map(sensors[i].score, 0, 
            sensors[i].minmax.avg_max - sensors[i].minmax.get_thold(), 
            //sensors[i].minmax.avg_max - sensors[i].minmax.avg_min, 
            1, 
            MAX_SPREAD);

  // Increase spread by time
  //Serial.printf("%d: %u - %u = %u\n", i, now, sensors[i].start, now - sensors[i].start);
  int n_white = 2 + (now - sensors[i].start) / 220;

  // Constrain to part of the strip
  if(n > MAX_SPREAD)
    n = MAX_SPREAD;
  if(n_white > MAX_SPREAD/5)
    n_white = MAX_SPREAD/5;

  int loc = sensors[i].location;

#if 0
  //EVERY_N_MILLISECONDS(500) {
    Serial.print("Strip ");
    Serial.print(i); 
    Serial.print(" triggered @ "); 
    Serial.print(loc); 
    Serial.print(" spread "); 
    Serial.print(n); 
    Serial.print(" delta "); 
    Serial.println(now - sensors[i].start);
  //}
#endif

  #if 0
  if(sensors[i].log.should_log()) {
    Serial.print("triggered, mux pin: "); 
    Serial.print(sensors[i].mux_pin); 
    Serial.print(" LED: "); 
    Serial.println(sensors[i].location);
  }
  #endif

  targets[loc] = CRGB::White;

  for(int i=1, nn=n; nn >= 0; nn--, i++) {
    uint8_t brightness = 255 - map(i, 1, n, 0, 100);

    int j = loc - i;
    int k = loc + i;

    if(i < n_white) {
      if(j >= 0) {
        targets[j] = CRGB::White;
        lifespans[j] = 250;
      }
      if(k < num_leds) {
        targets[k] = CRGB::White;
        lifespans[k] = 250;
      }
    }
    else {
      if(j >= 0) {
        targets[j] = rainbow.get(j, brightness);
        lifespans[j] = 250;
      }
      if(k < num_leds) {
        targets[k] = rainbow.get(k, brightness);
        lifespans[k] = 250;
      }
    }
  }

  blur1d(targets, num_leds, 160);
}

bool strip_t::near_mids(int lidx) {
  for(int i=0; i<nsens; i++)
    if(dist(lidx, sensors[i].location) < 5)
      return true;

  return false;
}

void strip_t::background_calc() {
  if(mstate.low_power.is_triggered())
    return;

  uint32_t now = millis();
  uint16_t activity = (mstate.active.avg * 100) / mstate.active.max_score;
  uint16_t my_activity = 0;

  // XXX Only update when in a state a pattern that might use it
  rainbow.update();

  xSemaphoreTake(mtx, portMAX_DELAY);
  for(int i=0; i<nsens; i++) {
    sensor_t &s = sensors[i];
    s.update(now);
    if(s.score) {
      update_triggered(now, i);
      my_activity += map(s.score, 0, s.minmax.get_max() - s.minmax.get_thold(), 0, 100);
    }
  }

  my_activity /= nsens;
  
  //if(my_activity && log.should_log()) 
  //  Serial.printf("my act: %u\n", my_activity);

  if(is_center)
    white.step(activity);

  if(!mstate.pulse.is_triggered()) {
    switch(mstate.pattern) {
        case 2:
        //if(is_center) 
          //waves.step(beatsin8(30, 15, 200), 10, my_activity);
          blob_asc.step(activity);
        //else // since not doing blobs, need to fade out any leftovers
        //  fadeToBlackBy(layer_waves, num_leds, 10); 

        do_basic_ripples(activity);
        break;
      case 0:
        //if(is_center)
          blobs.step(activity);
        //else // since not doing blobs, need to fade out any leftovers
        //  fadeToBlackBy(layer_waves, num_leds, 10);
       
        do_basic_ripples(activity);

       // do_high_energy_ripples(activity);
        break;
      case 1:
        if(is_center)  
          wave_pulse.step(my_activity);
        else {
          //blobs.step(activity);
          fadeToBlackBy(layer_waves, num_leds, 10);
        //do_basic_ripples(activity);
          do_high_energy_ripples(activity);
        }
        break;
      //  rainbow? 
      //  black ripples
      default:
        mstate.pattern = 0;
    }
  }

  xSemaphoreGive(mtx);
}

void strip_t::go_white() {
  for(int i=0; i<num_leds; i++) {
    targets[i] = CRGB::White; 
    lifespans[i] = 750;

    // Hack for better transitions
    layer_tracers[i] = layer_waves[i] = CRGB::Black;
  }
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
  if(mstate.active.is_triggered())
    return;
}

void strip_t::handle_low_power() {
  if(!mstate.low_power.is_triggered())
    return;

  // Fade out
  for(int i=0; i<num_leds; i++)
    targets[i] = CRGB::Black;
}

void strip_t::do_basic_ripples(uint16_t activity) {
  for(int i=0; i<nsens; i++)
    tracers_sens[i].step(activity);

  // high activity increases fade
  // 0 -> 3
  // 100 -> 20
  uint16_t fade = map(activity, 0, 100, 10, 30);
  fadeToBlackBy(layer_tracers, num_leds, fade);
}

void strip_t::do_high_energy_ripples(uint16_t activity) {
  for(int i=0; i<nsens; i++)
    tracers_sens[i].step(activity);
    
  for(int i=0; i<n_tracers; i++)
    tracers_rand[i].step(activity);

  uint16_t fade = map(activity, 0, 100, 15, 30);
  fadeToBlackBy(layer_tracers, num_leds, fade);
}

void strip_t::step() {
  uint32_t now = millis();
  uint32_t lapsed = now - last_update;

  //if(lapsed < 5)
  //  return;

  last_update = now;

  handle_low_activity();
  handle_low_power(); 
  // handle_pulse();

  xSemaphoreTake(mtx, portMAX_DELAY);
  for(int i=0; i<num_leds; i++) {
    if(lifespans[i] > 0) {
      if(lapsed > lifespans[i])
        lifespans[i] = 0;
      else {
        lifespans[i] -= lapsed;
        nblend(leds[i], targets[i], 10);
      } 
    }
    else {
      targets[i] = CRGB::Black;
      
      if(transition[i] == CRGB::White) {
        nblend(leds[i], transition[i], 80);
        //leds[i] = transition[i];
        
        // Let some tracer show through
        //if(layer_tracers[i] != CRGB::Black) {
        //  nblend(leds[i], layer_tracers[i], 30);
      }
      else {
        // Handle when we're fading out from a transition
        if(transition[i] != CRGB::Black) {
          nblend(leds[i], transition[i], 80);
          if(layer_tracers[i] != CRGB::Black)
            nblend(leds[i], layer_tracers[i], 100);
          nblend(leds[i], layer_waves[i], 20);
        }
        else {
          if(layer_tracers[i] != CRGB::Black)
            leds[i] = layer_tracers[i];
          else
            nblend(leds[i], layer_waves[i], 80);
        }
      }
    }
  }
  xSemaphoreGive(mtx);
}

void strip_t::find_mids() {
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
}
