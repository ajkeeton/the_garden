
#include "leds.h"

CRGB _leds[NUM_LEDS];

void leds_t::init() {
    leds = _leds;
    num_leds = NUM_LEDS;
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>
        (leds, num_leds).setCorrection(TypicalLEDStrip);

    set_brightness(255);
    clear();
}

void leds_t::step() {
    uint32_t now = millis();

    if (now - t_last_update < 50)
        return;

    t_last_update = now;
    show_rainbow();
    FastLED.show();
}