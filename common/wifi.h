#pragma once
#include <WiFi.h>
#include <WiFiMulti.h>

class wifi_t {
public:
    int status = WL_IDLE_STATUS;
    uint32_t retry_in = 0,
             last_log = 0;
    IPAddress garden_server;
    uint16_t port = 0;
    char name[128]; // client name for mDNS

    wifi_t() {
        strncpy(name, "[unset]", sizeof(name));
    }
    void init(const char *name);
    void init();
    void run();
    bool discover_server();
    bool send_msg(uint16_t type, uint16_t len, char *payload);
    bool send_msg(uint16_t type, const String &payload);
    bool send_sensor_msg(uint16_t type, uint16_t index, uint16_t value);
    bool read_msg();
};
