#pragma once
#include <Arduino.h>
#include <stdint.h>

#define BLINK_DELAY 750 // ms
#define LOG_TIMEOUT 500 // in ms

void blink();

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
