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
    strips[nstrips-1].trigger_both_directions = true;

    // Args:
    // - First mux pin with a sensor
    // - Total sensors
    // - What to call when a sensor triggers
    int num_sensors = 8;

    sensors.init(0, num_sensors, 
            ::on_sens_trigger_start,
            ::on_sens_trigger_cont, 
            ::on_sens_trigger_off,
            ::on_pir);

    sensors.add(0, 0, 45); // sensor on mux_pin 0, strip 0, LED 140
    sensors.add(0, 1, 60); // same sensor on mux_pin 0, strip 1, LED 130
    sensors.add(4, 0, 144/2); // mux_pin 7, strip 1, LED 120

    //sensors.add(1, 0, 90); // strip 0, sensor 1, mux pin 2, LED 90
    //sensors.add(2, 0, 230); // strip 0, sensor 2, mux pin 4, LED 230
    //sensors.add(3, 1, 128); // strip 1, mux pin 1, LED 128
    //sensors.add(4, 1, 195); // strip 1, mux pin 5, LED 195
    
    // PIR sensors get special treatment
    #ifdef MUX_PIN_PIR
    sensors.add_pir(MUX_PIN_PIR);
    #endif

    state.init(num_sensors);

    #ifdef LITTLE_WAD // XXX use dipswitch instead?
    inputs[0] = sensor_state_t(0, 0, 140);
    mstate.num_sens = 1;
    #else
    strips[nstrips++].init(_leds2, NUM_LEDS2);

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
              Serial.printf("Handling pulse message! %lu, %u, %u, %lu \n",
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
              state.handle_remote_update(msg.state.state_idx, msg.state.score);
              break;
          case PROTO_PIR_TRIGGERED:
              Serial.printf("Handling remote PIR message! %u\n", msg.pir.placeholder);
              state.on_remote_pir();
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
  EVERY_N_MILLISECONDS(1000) {
      //if(log_flags & LOG_STATE)
      state.log_info();
      //if(log_flags & LOG_SENSORS)
      sensors.log_info();
      //if(log_flags & LOG_MUX)
      sensors.mux.log_info();
      //Serial.printf("LED update: %lums. LED calcs: %lums. Sensors: %lums. Loop 0 (WiFi etc): %lums. WiFi read: %lums\n", 
      //    bench_led_push.avg, bench_led_calcs.avg, bench_sensors.avg, bloop0.avg, bench_wifi.avg);
      //if(log_flags & LOG_STRIPS)
      strips[0].log_info();
      //for(int i=0; i<nstrips; i++) strips[i].log_info();
  }
}