#pragma once
#include <WiFi.h>
#include <WiFiMulti.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>
#include <WiFiClient.h>
#include "common/proto.h"

// Should seldom have anything queued!
// The queue is to prevent wifi activity from slowing LED updates 
#define MAX_QUEUE 5 

struct msg_t {
    uint16_t full_len = 0; // full length of the message including header
    char buf[PROTO_HEADER_SIZE + PROTO_MAX_PAYLOAD];

    msg_t(uint32_t color, uint8_t fade, uint16_t spread, uint32_t delay) {
        int l = snprintf(buf + PROTO_HEADER_SIZE, sizeof(buf) - PROTO_HEADER_SIZE, 
            "%lu,%u,%u,%lu", color, fade, spread, delay);

        init_header(PROTO_PULSE, l);
    }

    msg_t(uint16_t strip, uint16_t led, uint16_t percent, uint32_t age) {
        int l = snprintf(buf + PROTO_HEADER_SIZE, sizeof(buf) - PROTO_HEADER_SIZE, 
            "%lu,%u,%lu", (uint32_t)strip << 16 | led, percent, age);

        init_header(PROTO_SENSOR, l);
    }

    void init_header(uint16_t type, int pay_len) {
        buf[0] = (type >> 8) & 0xFF;
        buf[1] = type & 0xFF;
        buf[2] = (PROTO_VERSION >> 8) & 0xFF;
        buf[3] = PROTO_VERSION & 0xFF;
        buf[4] = (pay_len >> 8) & 0xFF;
        buf[5] = pay_len & 0xFF;

        full_len = PROTO_HEADER_SIZE + pay_len;
    }

    ////////////////////////////////////
    // Convenience functions for logging
    uint16_t get_type() const {
        return (buf[0] << 8) | buf[1];
    }
    uint16_t get_version() const {
        return (buf[2] << 8) | buf[3];
    }
    uint16_t get_length() const {
        return (buf[4] << 8) | buf[5];
    }
    const char *get_payload() const {
        return &buf[PROTO_HEADER_SIZE];
    }
};

class wifi_t {
public:
    std::queue<msg_t> msgq;
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
    //bool send_msg(uint16_t type, uint16_t len, char *payload);
    bool send_msg(const char *buf, int len);
    bool send_msg(const msg_t &msg) {
        return send_msg(msg.buf, msg.full_len);
    }
    bool read_msg();

    void send_pulse(uint32_t color, uint8_t fade, uint16_t spread, uint32_t delay) {
        msg_t msg(color, fade, spread, delay);
        msg_push_queue(msg);
    }
    
    void send_sensor_msg(uint16_t strip, uint16_t led, uint16_t percent, uint32_t age) {
        //char msg[PROTO_MAX_PAYLOAD + PROTO_HEADER_SIZE];
        //int len = snprintf(msg, sizeof(msg) - PROTO_HEADER_SIZE, "%d,%d,%u,%lu", strip, led, percent, age);
        //msg_push_queue(PROTO_SENSOR, payload);
        msg_t msg(strip, led, percent, age);
        msg_push_queue(msg);
    }

    void msg_push_queue(const msg_t &msg) {
        if(!xSemaphoreTake(mtx, portMAX_DELAY)) {
            Serial.println("Failed to take mutex in msg_queue");
            return;
        }
        if(msgq.size() > 5) {
            Serial.println("Message queue is full, dropping first message");
            msgq.pop();
        }

        msgq.push(msg);
        xSemaphoreGive(mtx);
    }

};
