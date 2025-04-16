#define WADS_V2A_BOARD

#include <Wire.h>
#include <vector>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "common/wifi.h"

#include "common.h"

#define IN_BUTTON A2
#define IN_DIP A1

Mux_Read mux;
Wifi wifi;

SemaphoreHandle_t muxmtx = xSemaphoreCreateMutex();

Adafruit_SH1106G display(128, 64, &Wire, -1);
char rec_buf[128];  
int blink_delay = 500;

strip_t strips[6];
meta_state_t mstate;

#define num_strips() sizeof(strips)/sizeof(strips[0])

CRGB _leds1[NUM_LEDS1];
CRGB _leds2[NUM_LEDS2];
CRGB _leds3[NUM_LEDS3];
CRGB _leds4[NUM_LEDS4];
CRGB _leds5[NUM_LEDS5];
CRGB _leds6[NUM_LEDS5];

void oled_update() {
  #if 0
  EVERY_N_MILLISECONDS(500) {
    display.clearDisplay();
    display.setCursor(0, 1);
    display.printf("Pattern: %d\n", mstate.pattern);

    for(int i=0; i<mux_inputs[0].num_pins; i++) {
      if(i > 0) {
        //Serial.print(" | ");
      }

      //Serial.print(mux_inputs[0].get(i));

      display.printf("%3u ", mux_inputs[0].get(i));
      if(!((i+1)%5))
        display.println("");
    }
    //Serial.println();

    display.printf("\n total  |  active:\n");
    display.printf("    %2d | %2d /%2d\n",
      mstate.total, mstate.active.is_triggered(), mstate.active.score);

    display.printf(" pulse   |   low_pow:\n");
    display.printf("    %2d /%2d | %2d /%2d",
      mstate.pulse.is_triggered(), mstate.pulse.score, 
      mstate.low_power.is_triggered(), mstate.low_power.score);
  
    display.display();
  }
  #endif
}

void loop1() {
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

  mux.next();

  // XXX Revisit and cleanup
  mstate.total = 0;
  for(int i=0; i<num_strips(); i++) {
    strip_t &s = strips[i];
    xSemaphoreTake(s.mtx, portMAX_DELAY);
    for(int j=0; j<s.nsens; j++)
      mstate.total += s.sensors[j].score > 0;
    xSemaphoreGive(s.mtx);
  }

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
  
  oled_update();

  EVERY_N_MILLISECONDS(5) {
    for(int i=0; i<num_strips(); i++) {
      strips[i].background_calc();
    }
  }

  #ifdef LOG_LATENCY
  avg_lapsed = (millis() - now) + avg_lapsed * 9;
  avg_lapsed /= 10;
  #endif

  wifi.run();
}

void find_sens() {
  delay(100);
  for(int i=0; i<num_strips(); i++) {
    Serial.print("Strip "); Serial.print(i); Serial.print(": ");
    strips[i].find_mids();
  }
  Serial.println("---");
  FastLED.show();
}

void led_test() {
  for(int i=0; i<num_strips(); i++)
    strips[i].go_white();
  FastLED.show();
}

void loop() {
  blink();

  #ifdef FIND_SENS
  return find_sens();
  #endif

  #ifdef LED_TEST
  return led_test();
  #endif

  bool trigger_override = false;
  static uint16_t debounce = 0;
  EVERY_N_MILLISECONDS(5) {
    uint16_t but = analogRead(IN_BUTTON);
    if(but < 100) {
      switch(debounce) {
        case 3:
          Serial.println("Button pressed");
          // Treat as though sensor 1 is fully triggered
          trigger_override = true;
          debounce++; // Make sure we only trigger once
          break;
        case 4:
          trigger_override = true;
          break;
        default:
          debounce++;
      }
    }
    else
      debounce = 0;
  }

  // XXX make part of the backround_calc & sensor_update code
  mstate.total = 0;
  for(int i=0; i<num_strips(); i++) {
    if(trigger_override) {
      strips[i].sensors[0].score = strips[i].sensors[0].minmax.avg_max;
      strips[i].sensors[0].start = millis();
      strips[i].sensors[0].pending_pulse = true;
    }
    else {
      strips[i].update_triggered(millis(), 0);
    }
    for(int j=0; j<strips[i].nsens; j++)
      if(strips[i].sensors[j].score > 0)
         mstate.total++;
  }

  mstate.update();

  EVERY_N_MILLISECONDS(5000) {
    Serial.printf("Pattern %d. total / pulse / active / low_power: %d/%d\t %d/%d\t%d/%d/%d\t%d/%d\n",
      mstate.pattern,
      mstate.total, mstate.num_sens,
      mstate.pulse.is_triggered(), mstate.pulse.score,
      mstate.active.is_triggered(), mstate.active.score, mstate.active.avg, 
      mstate.low_power.is_triggered(), mstate.low_power.score);
  }

  uint32_t start1 = millis();

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

  uint32_t end1 = millis();
  uint32_t start2 = end1;
  
  FastLED.show();
  FastLED.delay(1);

  #ifdef LOG_LATENCY
  uint32_t end2 = millis();
  static uint32_t last = 0;
  if(millis() - last > 500) {
    Serial.printf("loop: %u / %u\n", end1 - start1, end2 - start2);
    last = millis();
  }
  #endif
}

void setup() {
  pinMode(IN_BUTTON, INPUT_PULLUP);
  pinMode(IN_DIP, INPUT_PULLUP);

  blink();

  Serial.begin(9600);

  //while(!Serial) {}

  #if 0
  Wire.setSDA(16);
  Wire.setSCL(17);

  if(!display.begin(0x3c, true)) {
    Serial.println("SSD1306 initialization failed");
  }
  else {
    Serial.println("Display initialized");
  }
  #endif

  mux.init();
  mstate.init(&mux);

  Serial.write("****");
  delay(250); // power-up safety delay (used in some of the fastled example code)
  
  strips[0].init(_leds1, NUM_LEDS1, true);
  strips[1].init(_leds2, NUM_LEDS2, true);
  strips[2].init(_leds3, NUM_LEDS3);
  strips[3].init(_leds4, NUM_LEDS4);
  strips[4].init(_leds5, NUM_LEDS5);
  strips[5].init(_leds6, NUM_LEDS5);

  // XXX Switch to variadic or pass some struct?
  strips[0].add_input(&mux, 0, 157);
  strips[0].add_input(&mux, 2, 90);
  strips[0].add_input(&mux, 4, 230);

  strips[1].add_input(&mux, 1, 128);
  strips[1].add_input(&mux, 5, 195);
  strips[1].add_input(&mux, 3, 280); 

  strips[2].add_input(&mux, 8, 105); // right
  strips[3].add_input(&mux, 7, 119); // front
  strips[4].add_input(&mux, 6, 100); // rear
  strips[5].add_input(&mux, 9, 115); // left

  // NOTE: the FastLED.addLeds has to be broken out like due to the template 
  // requiring a constant expression (LED_PINx)

  FastLED.addLeds<LED_TYPE, LED_PIN1, COLOR_ORDER>(_leds1, NUM_LEDS1).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<LED_TYPE, LED_PIN2, COLOR_ORDER>(_leds2, NUM_LEDS2).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<LED_TYPE, LED_PIN4, COLOR_ORDER>(_leds3, NUM_LEDS3).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<LED_TYPE, LED_PIN5, COLOR_ORDER>(_leds4, NUM_LEDS4).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<LED_TYPE, LED_PIN6, COLOR_ORDER>(_leds5, NUM_LEDS5).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<LED_TYPE, LED_PIN7, COLOR_ORDER>(_leds6, NUM_LEDS5).setCorrection(TypicalLEDStrip);

  FastLED.setBrightness(255); 
  FastLED.setMaxPowerInVoltsAndMilliamps(12,8500);

  FastLED.clear();

  #if 0
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setCursor(2, 5);
  display.println("Wadsworth'ing...");
  display.display();
  #endif

  Serial.println("Wadsworth'ing...");
}

void setup1() {
  wifi.init();
}
