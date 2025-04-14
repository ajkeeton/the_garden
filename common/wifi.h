#pragma once
#include <WiFi.h>
#include <WiFiMulti.h>

class Wifi {
public:
    int status = WL_IDLE_STATUS;
    uint32_t retry_in = 0,
             last_log;

    void init();
    void run();
};
