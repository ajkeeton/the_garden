#pragma once

#define WADS_V2A_BOARD
#include <map>
#include "common/common.h"
#include "common/mux.h"
#include "common/patterns.h"

//#define TEST_WHITE_ONLY
//#define FIND_SENS 1
//#define LED_TEST 1

#define LOG_TIMEOUT 500 // in ms

#define INIT_TRIG_THOLD 100 
#define INIT_TRIG_MAX 700
#define MAX_MAX_AVG 700 // Determined by 12v with my voltage dividers 
#define TRIG_SPREAD_MIN 250
#define TRIG_DECAY_DELTA_MIN 150 // ms
#define TRIG_DECAY_DELTA_MAX 500 // ms
#define MIN_TRIG_FOR_STATE_CHANGE 2
#define STATE_FALL_TIMEOUT 60000 // this many ms inactive and we transition down

#define MAX_RIPPLES_RAND 4

#define LED_PIN1    15
#define LED_PIN2    14
#define LED_PIN3    13
#define LED_PIN4    12
#define LED_PIN5    11
#define LED_PIN6    10
#define LED_PIN7    9
#define LED_PIN8    8

#define NUM_LEDS1   2*144 
#define NUM_LEDS2   2*144 
#define NUM_LEDS3   120 // 144
#define NUM_LEDS4   120 // 144
#define NUM_LEDS5   120 // 144
#define NUM_LEDS6   120 // 144

#define MAX_BRIGHT  255
#define LED_TYPE    WS2812
#define COLOR_ORDER RGB

#define DEF_HUE    225
#define DEF_SAT    255
#define DEF_BRIGHT 50

// For convenience with the state tracking
#define MAX_STRIPS 16
#define MAX_STRIP_LENGTH 4*144
#define MAX_MUX_IN 16
#define MUX_PIN_PIR 6

#define IN_BUTTON A2
#define IN_DIP A1

struct log_throttle_t {
  uint32_t last = 0;
  uint32_t log_timeout = LOG_TIMEOUT;
  bool should_log() {
    uint32_t now = millis();
    if(now - last < log_timeout)
      return false;

    last = now;
    return true;
  }
};
