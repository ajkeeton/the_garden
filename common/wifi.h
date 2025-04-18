#pragma once
#include <WiFi.h>
#include <WiFiMulti.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <WiFiClient.h> // Include WiFiClient for TCP communication
#include "common/proto.h"

class wifi_t {
public:
    int status = WL_IDLE_STATUS;
    uint32_t retry_in = 0,
             last_log = 0;
    IPAddress garden_server;
    uint16_t port = 0;
    char name[128]; // client name for mDNS
    SemaphoreHandle_t mtx = xSemaphoreCreateMutex();
    WiFiClient client; // TCP client for communication with the server

    wifi_t() {
        strncpy(name, "[unset]", sizeof(name));
    }
    void init(const char *name);
    void init();
    void run();
    bool discover_server();
    bool send_msg(uint16_t type, uint16_t len, char *payload);
    bool send_msg(uint16_t type, const String &payload);
    bool read_msg();

    void send_pulse(uint32_t color, uint8_t fade, uint16_t spread, uint32_t delay) {
        char payload[128];
        snprintf(payload, sizeof(payload), "%lu,%u,%u,%lu", color, fade, spread, delay);
        send_msg(PROTO_PULSE, payload);
    }
    
    void send_sensor_msg(int strip, int led, uint16_t percent, uint32_t age) {
        char payload[64];
        snprintf(payload, sizeof(payload), "%d,%d,%u,%lu", strip, led, percent, age);
        send_msg(PROTO_SENSOR, payload);
    }
};
