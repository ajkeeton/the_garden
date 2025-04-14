#pragma once

#include <FastLED.h>

#define LED_PIN 4
#define LED_TYPE WS2812B
#define COLOR_ORDER RGB
#define NUM_LEDS 12

class leds_t {
public:
    leds_t() : leds(nullptr), num_leds(0) {}
    leds_t(CRGB* leds, int num_leds) : leds(leds), num_leds(num_leds) {}
    
    void init();

    void set_all_leds(const CRGB& color) {
        for (int i = 0; i < num_leds; ++i) {
            leds[i] = color;
        }
        FastLED.show();
    }

    void set_led(int index, const CRGB& color) {
        if (index >= 0 && index < num_leds) {
            leds[index] = color;
            FastLED.show();
        }
    }

    void clear() {
        fill_solid(leds, num_leds, CRGB::Black);
        FastLED.show();
    }

    void set_brightness(uint8_t brightness) {
        FastLED.setBrightness(brightness);
        FastLED.show();
    }

    void show_rainbow() {
        for (int i = 0; i < num_leds; ++i) {
            leds[i] = CHSV(state++, 255, 255); // CHSV((i * 255 / num_leds), 255, 255);
        }
        FastLED.show();
    }

    void step();

private:
    CRGB* leds = nullptr;
    int num_leds = 0;
    uint32_t t_last_update = 0;
    uint32_t state = 0;
};