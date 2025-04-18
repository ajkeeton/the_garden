#include "wadsworth.h"
#include "common.h"
#include "state.h"

extern mux_t mux;
extern meta_state_t mstate;

CRGB _leds1[NUM_LEDS1];
CRGB _leds2[NUM_LEDS2];
CRGB _leds3[NUM_LEDS3];
CRGB _leds4[NUM_LEDS4];
CRGB _leds5[NUM_LEDS5];
CRGB _leds6[NUM_LEDS5];

void wad_t::init() {
    strips[nstrips++].init(_leds1, NUM_LEDS1, true);
    // Args:
    // - First mux pin with a sensor
    // - Total sensors
    // - What to call when a sensor triggers
    int num_sensors = 5;

    sensors.init(0, num_sensors, ::on_trigger);

    sensors.add(0, 0, 140); // strip 0, sensor 0, mux pin 0, LED 140
    sensors.add(1, 0, 90); // strip 0, sensor 1, mux pin 2, LED 90
    sensors.add(2, 0, 230); // strip 0, sensor 2, mux pin 4, LED 230
    sensors.add(3, 1, 128); // strip 1, mux pin 1, LED 128
    sensors.add(4, 1, 195); // strip 1, mux pin 5, LED 195

    state.init(num_sensors);

    #ifdef LITTLE_WAD // XXX use dipswitch instead?
    inputs[0] = sensor_state_t(0, 0, 140);
    mstate.num_sens = 1;
    #else
    strips[nstrips++].init(_leds2, NUM_LEDS2, true);
    strips[nstrips++].init(_leds3, NUM_LEDS3);
    strips[nstrips++].init(_leds4, NUM_LEDS4);
    strips[nstrips++].init(_leds5, NUM_LEDS5);
    strips[nstrips].init(_leds6, NUM_LEDS5);

    FastLED.addLeds<LED_TYPE, LED_PIN2, COLOR_ORDER>(_leds2, NUM_LEDS2).setCorrection(TypicalLEDStrip);
    FastLED.addLeds<LED_TYPE, LED_PIN4, COLOR_ORDER>(_leds3, NUM_LEDS3).setCorrection(TypicalLEDStrip);
    FastLED.addLeds<LED_TYPE, LED_PIN5, COLOR_ORDER>(_leds4, NUM_LEDS4).setCorrection(TypicalLEDStrip);
    FastLED.addLeds<LED_TYPE, LED_PIN6, COLOR_ORDER>(_leds5, NUM_LEDS5).setCorrection(TypicalLEDStrip);
    FastLED.addLeds<LED_TYPE, LED_PIN7, COLOR_ORDER>(_leds6, NUM_LEDS5).setCorrection(TypicalLEDStrip);
    #endif

    FastLED.addLeds<LED_TYPE, LED_PIN1, COLOR_ORDER>(_leds1, NUM_LEDS1).setCorrection(TypicalLEDStrip);

    FastLED.setBrightness(255); 
    FastLED.setMaxPowerInVoltsAndMilliamps(12,8500);

    FastLED.clear();

    // For logging
    for(int i=0; i<num_strips(); i++) {
        strips[i].id = i;
    }
}

void wad_t::next() {
    //for(int i=0; i<num_strips(); i++) {
    strips[0].step(state.score);
    //    strips[i].step();
    // }

    FastLED.show();
    FastLED.delay(1);

    #if 0
    uint16_t total_triggered = 0;
    uint32_t total_score = 0;

    for(int i=0; i<num_inputs; i++) {
        uint16_t raw = mux.read_raw(i);

        // Currently using the button to pretend we're triggered
        // And ignoring updates for anything but the first strip
        if(trigger_override && !i) {
            // Act as though first sensor is triggered
            raw = inputs[i].score.minmax.avg_max*1.5;
        }
        if(i)
            break;

        const score_t &score = inputs[i].update(i, raw);

        uint8_t strip_idx = inputs[i].strip;
        uint16_t led_idx = inputs[i].led;
        uint16_t sens_idx = inputs[i].sensor;

        strips[strip_idx].set_score(led_idx, sens_idx, score);

        if(score.value) {
            total_triggered++;
            total_score += score.value;
        }
    }

    EVERY_N_MILLISECONDS(1000) {
        Serial.print("Inputs: ");

        for(int i=0; i<num_inputs; i++) {
            Serial.printf("%3u\t| ", mux.read_raw(i));
        }

        Serial.print("\nScores: ");

        for(int i=0; i<num_inputs; i++) {
            Serial.printf("%3u & %u\t| ", inputs[i].score.value, inputs[i].score.duration);
        }

        Serial.println();

        inputs[0].score.minmax.debug();
    }

    // XXX
    // XXX currently ignoring total_score
    mstate.update(total_triggered, total_score);

return;

    uint16_t activity = (mstate.active.avg * 100) / mstate.active.max_score;
    // Ideally this would run periodically on the other core,
    // but only if wifi can be made non-blocking
    EVERY_N_MILLISECONDS(5) {
        for(int i=0; i<num_strips(); i++) {
            strips[i].update(activity);
        }
    }

    strips[0].step();
    for(int i=0; i<num_strips(); i++) {
        //strips[i].step();
    }

  #if 0 
  mstate.update();

  EVERY_N_MILLISECONDS(5000) {
    Serial.printf("Pattern %d. total / pulse / active / low_power: %d/%d\t %d/%d\t%d/%d/%d\t%d/%d\n",
      mstate.pattern,
      mstate.total, mstate.num_sens,
      mstate.pulse.is_triggered(), mstate.pulse.score,
      mstate.active.is_triggered(), mstate.active.score, mstate.active.avg, 
      mstate.low_power.is_triggered(), mstate.low_power.score);
  }

  // A hack but gets the job done
  if(strips[0].white.do_transition()) {
    Serial.println("Transitioning up");
    mstate.pattern++;
    for(int i=0; i<num_strips(); i++)
      strips[i].go_white();
  }
  else if(start1 - mstate.last_active > STATE_FALL_TIMEOUT) {
    Serial.println("Transitioning down");
    mstate.last_active = millis();
    mstate.pattern--;
  }

  for(int i=0; i<num_strips(); i++) {
    strips[i].step();
  }
  #endif

  FastLED.show();
  FastLED.delay(1);
  #endif
}