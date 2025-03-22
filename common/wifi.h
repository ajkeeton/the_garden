#pragma once
#include <WiFi.h>

class Wifi {
public:
    int status = WL_IDLE_STATUS;

    void init();
};

