#pragma once
#include <WiFi.h>

class Wifi {
public:
    int status = WL_IDLE_STATUS;
    uint32_t retry_in = 0;
    void init();
    void run();
};

