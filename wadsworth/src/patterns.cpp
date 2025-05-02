#include "common.h"
#include "gradient_palettes.h"
#include "common/wifi.h"
#include "patterns.h"

extern wifi_t wifi;

// Also, try ForestColors_p and HeatColors_p: https://fastled.io/docs/group___predefined_palettes.html
DEFINE_GRADIENT_PALETTE( remixed_Fuschia_7_gp ) {
    0,  43,  3, 153,
   63, 100,  4, 103,
  127, 188, 10, 105,
  191, 161, 11, 145,
  255, 135, 20, 220};

CRGBPalette16 palette_wave = Blue_Cyan_Yellow_gp;
CRGBPalette16 palette_ripple_target = Coral_reef_gp; // OceanColors_p;
CRGBPalette16 palette_ripple = Coral_reef_gp;
CRGBPalette16 palette_asc_blob = remixed_Fuschia_7_gp;

//extern meta_state_t mstate;

void setup_ripples_palette() {
  return;

  uint8_t thishue = random8();
  palette_ripple_target = CRGBPalette16(CHSV(thishue+random8(32), 255, random8(128,255)),
                                CHSV(thishue+random8(32), 255, random8(192,255)),
                                CHSV(thishue+random8(32), 192, random8(192,255)),
                                CHSV(thishue+random8(32), 255, random8(128,255)));                                
}

void rainbow_t::update() {
  //EVERY_N_MILLISECONDS(20) { hue++; } // slowly cycle the base color through the rainbow
  
  uint32_t now = millis();
  if(now - last_update > 5) { 
    hue++;
    last_update = now;
  }

  CHSV hsv;
  hsv.hue = hue;
  hsv.val = 230;
  hsv.sat = 240;
  for( int i = 0; i < nleds; ++i) {
    CRGB c = hsv;
    //if(lifespans[i])
    leds[i] = blend(leds[i], c, 100);
    //else
    //leds[i] = c;
    hsv.hue += 1;
  }
}

void tracer_t::step() {
    #if 0
    Serial.print(ripples[i].init_pos);
    Serial.print("\t");
    Serial.print(ripples[i].get_pos());
    Serial.print("\t");
    Serial.print(ripples[i].get_neg_pos());
    Serial.print("\t");
    Serial.println(ripples[i].life);
    #endif
    
    pos += velocity;
    life++;

    if(pos > num_leds - 1) {
      velocity *= -1;
      pos = num_leds - 1;
    }
 
    if(pos < 0) {
      velocity *= -1;
      pos = 0;
    }

    int k = get_pos();
    if(leds[k] == CRGB::Black)
      leds[k] =
          ColorFromPalette(palette_ripple, color, brightness, LINEARBLEND);
    else
      leds[k] =
          nblend(leds[k], ColorFromPalette(palette_ripple, color, brightness, LINEARBLEND), 125);

    k = get_neg_pos();
    leds[k] = 
        nblend(leds[k], ColorFromPalette(palette_ripple, color, brightness, LINEARBLEND), 125);

    brightness = scale8(brightness, fade);

    //if(life > max_life)
    //  exist = false;

    //if(brightness < 1) {
    if(life > max_life)
      exist = false;
      return;
    //}
}

void tracer_t::step(uint16_t activity) {
  EVERY_N_MILLISECONDS(80) {
    uint8_t maxChanges = 24;
    nblendPaletteTowardPalette(palette_ripple, palette_ripple_target, maxChanges);  
  }

  EVERY_N_SECONDS(5) {
    setup_ripples_palette();
  }

  // activity is a percentage
  // use to lower the delay
  // 0 -> 20ms delay
  // 100 -> 5ms delay

  uint32_t delay = map(100 - activity, 0, 100, 5, 20);
  uint32_t now = millis();

  if(now - last < delay)
    return;
  last = now;

  if(!exist && random8() > 250)
    // first param controls how fast the brightness drops. Higher is longer
    reset(random8(240,254), random(100,350), -1); // random8()&1 ? 1 : -1);
  
  if(exist)
    step();
}

void animate_waves_t::step(uint8_t brightness, uint16_t delay, uint16_t wave_mult) {
  uint32_t now = millis();
  if(now - last_update < delay)
    return;
  last_update = now;

  wave_offset += 1 + wave_mult/20;

  //Serial.printf("wm: %u eased: ", wave_mult);
  // as of 09.24, wave_mult is a percentage
  // apply "easing" - taper higher values
  uint8_t mapped = map(wave_mult, 0, 100, 0, 255);
  uint8_t eased = ease8InOutQuad(mapped);
  wave_mult = lerp8by8(0, 100, eased);
  
  // uint8_t theta = beatsin8(30, 1, 255); // 20, max(20, brightness));
  for(int i=0; i<num_leds; i++) {
    uint8_t theta = map(i * wave_mult, 0, 2*num_leds, 20, 255);
    uint8_t b = ease8InOutQuad(sin8(theta + wave_offset));
    b = map(b, 0, 255, 30, 200);
    leds[i] = ColorFromPalette(palette_wave, i+wave_offset, b, LINEARBLEND);
  }
}

void tracer_v2_pulse_t::step() {
  if(!exist)
    return;

  uint32_t now = millis();

  if(now - t_last_update < t_update_delay)
    return;

  t_last_update = now;

  uint32_t _spread = spread;

  if(reverse) {
    if(pos == (num_leds - 1) && (leds[num_leds-2] == CRGB::Black)) {
      exist = false;
      return;
    }

    if(pos < spread)
      _spread = pos;
    else if(num_leds - pos < spread)
      _spread = num_leds - pos;
      
    //Serial.printf("reverse tracer move? %lu now - last_update: %lu < %lu\n", 
    //    pos, now - t_last_update, t_update_delay);
  }
  // We're finished when !pos && pos+1 has faded to black
  // The reason for pos+1 is in case we change the LED index before updating
  // the color. This reeeeeally shoulnd't happen, but just in case...
  else {
    if(!pos) {//} && leds[1] == CRGB::Black) {
      // Tell the gardener that we're done so it can forward the pulse to the others
      wifi.send_pulse(
          color,
          fade,
          spread,
          t_update_delay);
      exist = false;
      // Serial.printf("Sending pulse: %lu, %lu, %lu, %lu\n", color, fade, spread, t_update_delay);   
      return;
    }

    if(pos < spread)
      _spread = pos;
  }

  for(int i=0; i < _spread; i++) {
    int pi = pos+i;
    if(pi >= num_leds)
      break;

    if(leds[pi] == CRGB::Black)
      leds[pi] = CRGB::White;
    else
      leds[pi] = nblend(leds[pi], CRGB::White, 125);
  }

  fadeToBlackBy(leds, num_leds, fade);
  //Serial.printf("tracer at %lu, %u, pos < num_leds - 1: %d\n", 
  //                pos, leds[pos>0 ? pos - 1: pos].getLuma(), pos < num_leds-1);

  if(reverse) {
    if(pos < num_leds-1)
      pos++;
  }
  else
    if(pos > 0)
      pos--;
  //if(reverse) Serial.println("[reverse exit]");
}

void wave_pulse_t::step(uint16_t activity) {
  uint32_t now = millis();
  if(now - last_update < delay_osc)
    return;

  last_update = now;

  if(spread < max_spread) {
    spread++;
  }
  else {
    if(now - start > max_age)
      fadeToBlackBy(leds, num_leds, 10);
    
    if(leds[center] == CRGB::Black)
      reset();
  }

  for(int i=0; spread != max_spread && i<spread/2; i++) {
    int x = i + center;
    if(x >= num_leds)
      x = num_leds-1;

    leds[x] = blend(leds[x], ColorFromPalette(palette_wave, i+color_offset, 255, LINEARBLEND), 100);

    x = center - i;

    if(x < 0)
      x = 0;
    
    leds[x] = blend(leds[x], ColorFromPalette(palette_wave, i+color_offset, 255, LINEARBLEND), 100);
  }

  blur1d(leds, num_leds, 160);
}

void blob_t::step(uint16_t activity) {
  uint32_t now = millis();

  CRGB color = CRGB::Blue; // TODO: change color with activity level
  color.r = map(activity, 0, 100, 0, 255);
  color.b -= activity/2;

  if(spread < max_spread) {
    spread++;
  }
  else {
    if(now - start > max_age) {
      int i = min(center + spread/2, num_leds);
      fadeToBlackBy(&leds[center], i, 10);
      if(spread/2 > center)
        fadeToBlackBy(leds, center, 10);
      else
        fadeToBlackBy(&leds[center - spread/2], center - spread/2, 10);
    }
    
    if(leds[center] == CRGB::Black)
      reset();
    return;
  }

  for(int i=0; spread != max_spread && i<spread/2; i++) {
    int x = i + center;
    if(x >= num_leds)
      x = num_leds-1;

    leds[x] = blend(leds[x], color, 100);

    x = center - i;

    if(x < 0)
      x = 0;

    leds[x] = blend(leds[x], color, 100);
  }
}

void tracer_blob_t::step(uint16_t act) {
    if(!exist) {
      if(random8() > 250)
        reset(5);
      return;
    }
    
    pos += velocity;
    life++;
   
    if(pos >= num_leds) {
      exist = false;
      return;
    }

    leds[pos] = ColorFromPalette(palette_asc_blob, pos+color_offset, brightness, LINEARBLEND);
}

void climb_white_t::step(uint16_t activity) {
  uint32_t now = millis();
  if(now < rest_until) {
    if(pos > 0)
      pos--;
    fadeToBlackBy(leds, num_leds, 5);
    return;
  }
  
  if(now - last < 15)
    return;

  if(!pos)
    time_trans = 0;

  last = now;
  activity = activity/10+1;

  if(activity < 5) { // XXX || mstate.total < MIN_TRIG_FOR_STATE_CHANGE) {
    fadeToBlackBy(leds, num_leds, 5);
    if(pos > 0)
      pos--;
    return;
  }

  if(pos >= num_leds) {
    rest_until = millis() + TRANS_TIMEOUT;
    return;
  }

  leds[pos++] = CRGB::White;
}

#if 0
bool climb_white_t::in_trans() {
  uint32_t now = millis();
  return mstate.total >= MIN_TRIG_FOR_STATE_CHANGE && now > rest_until;
}
#endif

bool climb_white_t::do_transition() {
  //if(log.should_log())
  EVERY_N_MILLISECONDS(500) {
    Serial.printf("White: %u of %u. time_trans: %u\n", pos, num_leds, time_trans);
  }

  if(pos < num_leds-1)
    return false;
  
  if(!time_trans) {
    time_trans = millis();
    return true;
  }

  return false;
}