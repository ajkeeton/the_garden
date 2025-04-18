#define WADS_V2A_BOARD

#include <Wire.h>
#include <vector>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "wadsworth.h"
#include "common/wifi.h"
#include "common.h"
#include "state.h"

wad_t wads;
wifi_t wifi;

SemaphoreHandle_t muxmtx = xSemaphoreCreateMutex();

int blink_delay = 500;

void on_trigger(int strip, int led, const sensor_state_t &s) {
  wads.on_trigger(strip, led, s);
}

bool button_pressed() {
  static uint16_t debounce = 0;
  EVERY_N_MILLISECONDS(5) {
      uint16_t but = analogRead(IN_BUTTON);
      if(but < 100) {
          switch(debounce) {
              case 3:
                  Serial.println("Button pressed");
                  debounce++;
                  return true;
              case 4:
                  return true;
              break;
              default:
                  debounce++;
          }
      }
      else
          debounce = 0;
  }

  return debounce >= 3;
}

void loop() {
  static uint32_t avg_lapsed = 0;
  static bool log = true;
  static int lastl = 0;

  #ifdef LOG_LATENCY
  uint32_t now = millis();
  if(now - lastl > 500) {
    Serial.printf("loop1: %u\n", avg_lapsed);
    lastl = now;
  }
  #endif

  #if 0
  // XXX Revisit and cleanup
  mstate.total = 0;
  for(int i=0; i<num_strips(); i++) {
    strip_t &s = strips[i];
    xSemaphoreTake(s.mtx, portMAX_DELAY);
    for(int j=0; j<s.nsens; j++)
      mstate.total += s.sensors[j].score > 0;
    xSemaphoreGive(s.mtx);
  }
  #endif

  #ifdef LOG_MUX
  EVERY_N_MILLISECONDS(500) {
    for(int i=0; i<16; i++) {
      if(i > 0) {
        Serial.print(" | ");
      }

      Serial.printf("%3u ", mux.read_raw(i)); 
    }
    Serial.println();
  }
  #endif

  #ifdef LOG_LATENCY
  avg_lapsed = (millis() - now) + avg_lapsed * 9;
  avg_lapsed /= 10;
  #endif

  wifi.run();
  wads.next_core_0();
}

void loop1() {
  blink();

  if(::button_pressed()) { // XXX Move to other core?
    wads.button_override();
  }

  #ifdef FIND_SENS
  return find_sens();
  #endif

  #ifdef LED_TEST
  return led_test();
  #endif

  wads.next_core_1();
}

void setup1() {
  pinMode(IN_BUTTON, INPUT_PULLUP);
  pinMode(IN_DIP, INPUT_PULLUP);

  blink();

  Serial.begin(9600);

  //while(!Serial) {}

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
  wifi.init("wadsworth");
}
