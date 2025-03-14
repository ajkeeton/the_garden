#include "common.h"
#include <Arduino.h>

void blink() {
    static bool onoff = false;
    static uint32_t last = 0;

    uint32_t now = millis();
    if(now - last < BLINK_DELAY) 
        return;

    last = now;
    onoff = !onoff;
    digitalWrite(LED_BUILTIN, onoff);
}
