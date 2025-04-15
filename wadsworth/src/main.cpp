#include <Wire.h>
#include <vector>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

//#include <U8g2lib.h>
#include "common.h"

SemaphoreHandle_t muxmtx = xSemaphoreCreateMutex();

Adafruit_SH1106G display(128, 64, &Wire, -1);
char rec_buf[128];  
int blink_delay = 500;

mux_input_t mux_inputs[1];
strip_t strips[6];
meta_state_t mstate;

#define num_strips() sizeof(strips)/sizeof(strips[0])

CRGB _leds1[NUM_LEDS1];
CRGB _leds2[NUM_LEDS2];
CRGB _leds3[NUM_LEDS3];
CRGB _leds4[NUM_LEDS4];
CRGB _leds5[NUM_LEDS5];
CRGB _leds6[NUM_LEDS5];

void blink() {
  static bool state = false;
  static uint32_t last = 0;

  if(millis() - last > 250) {
    state = !state;
    digitalWrite(LED_BUILTIN, state); 
    last = millis();
  }
}

void oled_update() {
  return;
  
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

  #if 0
  display.firstPage();
  do {
    display.setFont(u8g2_font_ncenB14_tr);
    display.drawStr(0,24,"Hello World!");
  } while (display.nextPage());
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

  mux_inputs[0].update();
  // XXX Revisit and cleanup
  mstate.total = 0;
  for(int i=0; i<num_strips(); i++) {
    strip_t &s = strips[i];
    xSemaphoreTake(s.mtx, portMAX_DELAY);
    for(int j=0; j<s.nsens; j++)
      mstate.total += s.sensors[j].score > 0;
    xSemaphoreGive(s.mtx);
  }

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
  //for(int i=0; i<num_strips(); i++)
  //  strips[i].handle_grow_white();
  //FastLED.show();
  display.printf("LED test not implemented");
  display.display();
}

void loop() {
  blink();

  #ifdef FIND_SENS
  return find_sens();
  #endif

  #ifdef LED_TEST
  return led_test();
  #endif

  // make part of the backround_calc & sensor_update code
  mstate.total = 0;
  for(int i=0; i<num_strips(); i++)
    for(int j=0; j<strips[i].nsens; j++)
      if(strips[i].sensors[j].score > 0)
         mstate.total++; 

  mstate.update();

  EVERY_N_MILLISECONDS(500) {
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
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(MUX1, OUTPUT);
  pinMode(MUX2, OUTPUT);
  pinMode(MUX3, OUTPUT);
  pinMode(MUX4, OUTPUT);
  pinMode(MUX_EN, OUTPUT);

  blink();

  Serial.begin(9600);

  //while(!Serial) {}

  Wire.setSDA(16);
  Wire.setSCL(17);

  delay(250);

  #if 0
  if(!display.begin(0x3c, true)) {
    Serial.println("SSD1306 initialization failed");
  }
  else {
    Serial.println("Display initialized");
  }
  #endif

  mux_inputs[0].init(MUX_IN1);
  mstate.init(&mux_inputs[0]);

  // Wire.onReceive(on_receive);
  // Wire.onRequest(on_request);

  Serial.write("****");
  delay(250); // power-up safety delay (used in some of the fastled example code)
  
  strips[0].init(_leds1, NUM_LEDS1, true);
  strips[1].init(_leds2, NUM_LEDS2, true);
  strips[2].init(_leds3, NUM_LEDS3);
  strips[3].init(_leds4, NUM_LEDS4);
  strips[4].init(_leds5, NUM_LEDS5);
  strips[5].init(_leds6, NUM_LEDS5);

  // XXX Switch to variadic or pass some struct?
  strips[0].add_input(&mux_inputs[0], 0, 157);
  strips[0].add_input(&mux_inputs[0], 2, 90);
  strips[0].add_input(&mux_inputs[0], 4, 230);

  strips[1].add_input(&mux_inputs[0], 1, 128);
  strips[1].add_input(&mux_inputs[0], 5, 195);
  strips[1].add_input(&mux_inputs[0], 3, 280); 

  strips[2].add_input(&mux_inputs[0], 8, 105); // right
  strips[3].add_input(&mux_inputs[0], 7, 119); // front
  strips[4].add_input(&mux_inputs[0], 6, 100); // rear
  strips[5].add_input(&mux_inputs[0], 9, 115); // left

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