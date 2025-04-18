
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

void on_sens_trigger_start(int strip, int led, const sensor_state_t &s) {
  wads.on_sens_start(strip, led, s);
}

void on_sens_trigger_cont(int strip, int led, const sensor_state_t &s) {
  wads.on_sens_cont(strip, led, s);
}

void on_sens_trigger_off(int strip, int led, const sensor_state_t &s) {
  wads.on_sens_off(strip, led, s);
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
  bloop0.start();
  // XXX if wifi becomes noblocking, put mux.next() here (removed from state code)
  wifi.run();
  wads.next_core_0();
  bloop0.end();
}

void loop1() {
  // NOTE: intentionally keeping blink the wifi core. 
  // That way we have info coming from cores, and if one locks up, we can tell which
  blink(); 

  switch(button_state()) { // XXX Move to other core?
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

bool ready_core_0 = false;

void setup1() {
  while(!ready_core_0) {}

  pinMode(IN_BUTTON, INPUT_PULLUP);
  pinMode(IN_DIP, INPUT_PULLUP);

  blink();

  wads.init();
  
  delay(250); // power-up safety delay (used in some of the fastled example code)

  // NOTE: the FastLED.addLeds has to be broken out like due to the template 
  // requiring a constant expression (LED_PINx)

  #if 0
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setCursor(2, 5);
  display.println("Wadsworth'ing...");
  display.display();
  #endif

  Serial.println("Wadsworth'ing...");
}

void setup() {
  Serial.begin(9600);

  uint32_t now = millis();
  // Wait up to 2 seconds for serial to be ready
  while(!Serial && (millis() - now < 2000)) { }
  delay(250);
  ready_core_0 = true;
  wifi.init("wadsworth");
}
