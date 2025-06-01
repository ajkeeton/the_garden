#include "leds.h"

DEFINE_GRADIENT_PALETTE( BCY ) {
    0,   0,  0,255,
   63,   0, 55,255,
  127,   0,255,255,
  191,  42,255, 45,
  255, 255,255,  0};

static CRGBPalette16 palette_wave = BCY;
CRGB _leds[NUM_LEDS];

void leds_t::init() {
    leds = _leds;
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>
        (leds, num_leds).setCorrection(TypicalLEDStrip);

    set_brightness(255);

    set_all_leds(CRGB::Black);

    layer_colored_glow.init(num_leds, true);
    layer_waves.init(num_leds, false);
    
    // These are the LED animations themselves
    pod.init(layer_colored_glow.targets, num_leds);
    waves.init(layer_waves.targets, num_leds);
    wave_pulse.init(layer_waves.targets, num_leds);
    blobs.init(layer_waves.targets, num_leds);
}

void leds_t::step() {
    uint32_t now = millis();

    if (now - t_last_update < 1) {
        FastLED.delay(1);
        return;
    }
    if (now - t_last_update < 10) {
        return;
    }

    t_last_update = now;

    for(int i=0; i<num_leds; i++) {
    #warning figure out best to additively blend layers
      //layer_colored_glow.blend(leds[i], i, 255);
      //layer_waves.blend(leds[i], i, 20);

      leds[i] = blend(leds[i], layer_colored_glow.targets[i], 20);
    }
  
    layer_colored_glow.fade(5);
    FastLED.show();
}

// Update the layers, but don't push to the LEDs
// XXX From Wadsworth
void leds_t::background_update() {
    uint16_t global_activity = 50; // XXX Get from gardener messages
    pod.step();

  switch(pattern_idx) {
    case 0:
        blobs.step(global_activity);

      // do_high_energy_ripples(activity);
      break;
    case 1:
        layer_waves.fade(10);
      break;
    default:
      pattern_idx = 0;
    }
}


void venus_pod_t::step_rainbow() {
    if(!ready())
        return;

    uint32_t now = millis();

    for (int i = 0; i < num_leds; ++i) {
        leds[i] = CHSV((i * 255 / num_leds + now / 10) % 255, 255, 255);
    }
}

void venus_pod_t::step() {
    if(!ready())
        return;

    EVERY_N_MILLISECONDS(10) {
        c_offset++;
        //c_offset = random(0, 255);
    }

    for (int i = 0; i < num_leds; ++i) {
        leds[i] = blend(leds[i], 
                ColorFromPalette(palette_wave, i*2+c_offset, 255, LINEARBLEND), 10);
    }
}