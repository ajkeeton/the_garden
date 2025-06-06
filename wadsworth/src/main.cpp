
//#include <Wire.h>
//#include <Adafruit_GFX.h>
//#include <Adafruit_SH110X.h>
#include "wadsworth.h"
#include "common.h"
#include "common/wifi.h"

#include "state.h"

wad_t wads;
wifi_t wifi;
benchmark_t bloop0;

int blink_delay = 500;
// To make sure things don't start running until after core 0 has finished setup()
bool ready_core_0 = false;

void on_sens_trigger_start(int strip, int led, const sensor_state_t &s) {
  wads.on_sens_start(strip, led, s);
}

void on_sens_trigger_cont(int strip, int led, const sensor_state_t &s) {
  wads.on_sens_cont(strip, led, s);
}

void on_sens_trigger_off(int strip, int led, const sensor_state_t &s) {
  wads.on_sens_off(strip, led, s);
}

void on_pir(int pir_idx) {
  wads.on_pir(pir_idx);
}

int button_state() {
  static uint16_t debounce = 0;
  EVERY_N_MILLISECONDS(5) {
      uint16_t but = analogRead(IN_BUTTON);
      if(but < 100) {
          switch(debounce) {
              case 3:
                  Serial.println("Button pressed");
                  debounce++;
              case 4:
                  break;
              break;
              default:
                  debounce++;
          }
      }
      else
          debounce = 0;
  }

  return debounce;
}

void loop() {
  // NOTE: intentionally keeping blink the wifi core. 
  // That way we have info coming from cores, and if one locks up, we can tell which
  blink();
  
  bloop0.start();
  wifi.next();
  wads.next_core_0();

  bloop0.end();
}

void loop1() {
  blink(); 

  switch(button_state()) {
    case 3:
      wads.button_pressed();
      break;
    case 4:
      wads.button_hold();
    default: // debouncing
      break;
  }

  #ifdef FIND_SENS
  return find_sens();
  #endif

  #ifdef LED_TEST
  return led_test();
  #endif

  wads.next_core_1();
}

void wait_serial() {
  uint32_t now = millis();
  // Wait up to 2 seconds for serial to be ready, otherwise just move on...
  while(!Serial && (millis() - now < 2000)) { 
    blink();
    delay(50);
  }
}

void setup1() {
  pinMode(IN_BUTTON, INPUT_PULLUP);
  pinMode(IN_DIP, INPUT_PULLUP);

  while(!ready_core_0) {}

  // For some reason this needs to be called for both cores?!
  // Otherwise, even with the check for ready_core_0, we just sail right on 
  // without serial ready
  wait_serial(); 

  bool full_wadsworth = false;
  if(analogRead(IN_DIP) < 512) {
    full_wadsworth = true;
    Serial.println("Full Wadsworth enabled by DIP");
  }

  #ifdef THE_REAL_WADSWORTH
  full_wadsworth = true;
  Serial.println("Full Wadsworth hardcoded on");
  #endif

  wads.init(full_wadsworth);

  Serial.println("Wadsworth'ing...");
}

void setup() {
  blink();

  Serial.begin(9600);

  wait_serial();

  wifi.init("wadsworth");
  ready_core_0 = true;
}
