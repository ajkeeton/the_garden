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
CRGB _leds6[NUM_LEDS6];

void wad_t::init(bool full_wadsworth) {
    full_wadsworth ? init_full() : init_deco();

    FastLED.setBrightness(255); 
    FastLED.setMaxPowerInVoltsAndMilliamps(12,8500);
    FastLED.clear();

    // Each strip has an ID to distinguish them in log messages
    for(int i=0; i<num_strips(); i++) {
        strips[i].id = i;
    }

    #ifdef TEST_WHITE_ONLY
    // NOTE: with the current LED strips, all black pulls around 0.35A, all white 1.3A
    while(true) {
        for(int i=0; i<num_strips(); i++) {
            strips[i].go_white();
        }
        FastLED.show();
        FastLED.delay(1000);
    }
    #endif
}

void wad_t::init_full() {
    wadsworth_is_full = true;

    int num_sensors = 1;
    sensors.init(0, num_sensors, 
            ::on_sens_trigger_start,
            ::on_sens_trigger_cont, 
            ::on_sens_trigger_off,
            ::on_pir);
    state.init(num_sensors);

    // Args:
    // - Mux pin with the sensor
    // - The corresponding strip
    // - What to call when a sensor triggers
    sensors.add(0, 5, 144); // sensor on mux_pin 0, strip 5, LED 144
    sensors.add(0, 6, 144); // same sensor on mux_pin 0, strip 6, LED 144
    
    //sensors.add(4, 0, 144/2); // mux_pin 7, strip 1, LED 120
    //sensors.add(1, 0, 90); // strip 0, sensor 1, mux pin 2, LED 90
    //sensors.add(2, 0, 230); // strip 0, sensor 2, mux pin 4, LED 230
    //sensors.add(3, 1, 128); // strip 1, mux pin 1, LED 128
    //sensors.add(4, 1, 195); // strip 1, mux pin 5, LED 195

    strips[nstrips++].init(_leds1, NUM_LEDS1, true);
    strips[nstrips++].init(_leds2, NUM_LEDS2, true);
    strips[nstrips++].init(_leds3, NUM_LEDS3, true);
    strips[nstrips++].init(_leds4, NUM_LEDS4, true);
    strips[nstrips++].init(_leds5, NUM_LEDS5, true);

    FastLED.addLeds<LED_TYPE, LED_PIN1, COLOR_ORDER>(_leds1, NUM_LEDS2).setCorrection(TypicalLEDStrip);
    FastLED.addLeds<LED_TYPE, LED_PIN2, COLOR_ORDER>(_leds2, NUM_LEDS3).setCorrection(TypicalLEDStrip);
    FastLED.addLeds<LED_TYPE, LED_PIN3, COLOR_ORDER>(_leds3, NUM_LEDS4).setCorrection(TypicalLEDStrip);
    FastLED.addLeds<LED_TYPE, LED_PIN4, COLOR_ORDER>(_leds4, NUM_LEDS5).setCorrection(TypicalLEDStrip);
    FastLED.addLeds<LED_TYPE, LED_PIN5, COLOR_ORDER>(_leds5, NUM_LEDS5).setCorrection(TypicalLEDStrip);
}

void wad_t::init_deco() {
    wadsworth_is_full = false;

    strips[nstrips++].init(_leds1, NUM_LEDS1, true);
    strips[nstrips++].init(_leds2, NUM_LEDS2, true);

    FastLED.addLeds<LED_TYPE, LED_PIN1, COLOR_ORDER>(_leds1, NUM_LEDS1).setCorrection(TypicalLEDStrip);
    FastLED.addLeds<LED_TYPE, LED_PIN2, COLOR_ORDER>(_leds2, NUM_LEDS2).setCorrection(TypicalLEDStrip);

    // Args are mux pin start and mux pin end for sensors
    sensors.init(4, 5,
            ::on_sens_trigger_start,
            ::on_sens_trigger_cont, 
            ::on_sens_trigger_off,
            ::on_pir);

    // Init with 2 sensors
    state.init(2);

    sensors.add(4, 0, 144); // sensor on mux_pin 5, strip 0, LED 144
    sensors.add(4, 1, 144); // same sensor on mux_pin 5, strip 1, LED 144
    
    // PIR sensors get special treatment
    #ifdef MUX_PIN_PIR
    sensors.add_pir(MUX_PIN_PIR);
    #endif
}

void wad_t::strips_next() {
   for(int i=0; i<num_strips(); i++) {
      strips[i].step(state.score);
   }

  FastLED.show();
  FastLED.delay(1);

  #if 0 
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
}

void wad_t::next_core_1() {
  log_info();

  // Check for any commands from the garden server
  bench_wifi.start();
  recv_msg_t msg;
  if(wifi.recv_pop(msg)) {
      switch(msg.type) {
          case PROTO_PULSE:
              Serial.printf("Handling pulse message! color: %lu, fade: %u, spread: %u, delay: %lu\n",
                  msg.pulse.color, msg.pulse.fade, msg.pulse.spread, msg.pulse.delay);
              for(int i=0; i<nstrips; i++) {
                  strips[i].handle_remote_pulse(
                      msg.pulse.color, 
                      msg.pulse.fade, 
                      msg.pulse.spread, 
                      msg.pulse.delay);
              }
              break;
          case PROTO_STATE_UPDATE:
              // XXX There might not be reason to track the pattern_idx in the state
              state.handle_remote_update(msg.state.pattern_idx, msg.state.score);
              break;
          case PROTO_PIR_TRIGGERED:
              Serial.printf("Handling remote PIR message! %u\n", msg.pir.placeholder);
              state.on_remote_pir();
              break;
          case PROTO_SLEEPY_TIME:
              Serial.println("Received sleep command from server");
              break;
          default:
              Serial.printf("Ignoring message with type: 0x%X\n", msg.type);
              break;
      }
  }
  bench_wifi.end();

  state.next(); 

  /////////////////////////////
  // Sensor updates
  // XXX If this moves cores, don't forget the mutex for on_trigger
  bench_sensors.start();
  sensors.next();
  bench_sensors.end();

  bench_led_calcs.start();
  // XXX If wifi can be made non-blocking, move this to other core
  for(int i=0; i<nstrips; i++) {
    strips[i].background_update(state);
  }
  bench_led_calcs.end();

  bench_led_push.start();
  strips_next();
  bench_led_push.end();
}

void wad_t::log_info() {
  EVERY_N_MILLISECONDS(500) {
      //if(log_flags & LOG_STATE)
      state.log_info();
      //if(log_flags & LOG_SENSORS)
      //sensors.log_info();
      //if(log_flags & LOG_MUX)
      
      sensors.log_info();

      //sensors.mux.log_info();
      //Serial.printf("LED update: %lums. LED calcs: %lums. Sensors: %lums. Loop 0 (WiFi etc): %lums. WiFi read: %lums\n", 
      //    bench_led_push.avg, bench_led_calcs.avg, bench_sensors.avg, bloop0.avg, bench_wifi.avg);
      
      //if(log_flags & LOG_STRIPS)
      //strips[0].log_info();
      //for(int i=0; i<nstrips; i++) strips[i].log_info();
  }
}