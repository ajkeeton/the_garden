#pragma once
#include <WiFi.h>
#include <WiFiMulti.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <WiFiClient.h>
#include "common/proto.h"

// Note on the mutexes:
// https://www.raspberrypi.com/documentation/pico-sdk/high_level.html#group_mutex_1gabe21d7ce624db2df7afe86c4bba400a2

// Should seldom have anything queued!
// The queue is to prevent wifi activity from slowing LED updates 
#define MAX_QUEUE 16

// Close connection to the server if it has been quiet for too long.
// When we lose and regain WiFi the connection doesn't reset. We wind up 
// listening for messages but won't receive any
#define TIMEOUT_SERVER 60*1000 

struct recv_msg_t {
    uint16_t type = 0;

    union {
        msg_pulse_t pulse;
        msg_state_t state;
        msg_pir_t pir;
    }; 

    recv_msg_t() {}
    recv_msg_t(uint16_t t) : type(t) {}
    // For the sleepy time message
    recv_msg_t(const msg_pulse_t &p) : type(PROTO_PULSE), pulse(p) {}
    recv_msg_t(const msg_state_t &s) : type(PROTO_STATE_UPDATE), state(s) {}
    recv_msg_t(const msg_pir_t &s) : type(PROTO_PIR_TRIGGERED), pir(s) {}
};

class wifi_t {
public:
    std::queue<msg_t> msgq_send;
    std::queue<recv_msg_t> msgq_recv;

    uint32_t retry_in = 0,
             last_log = 0,
             last_retry = 0,
             // With the WiFi going up and down we sometimes don't realize 
             // the connection is gone. Now we expect periodic pings from the 
             // server or else we close the connection and reconnect
             t_last_server_contact = 0,
             t_last_scan = 0;
    uint16_t port = 7777;
    char name[128]; 
    mutex_t mtx;
    WiFiClient client;

    wifi_t() {
        strncpy(name, "[unset]", sizeof(name));
        mutex_init(&mtx);
    }
    
    // Initialize with the name to be used when the client identifies itself
    void init(const char *name);
    void init();

    // Call with each loop
    void next();

    // Raw send
    bool send(const char *buf, int len);
    // Send message struct
    bool send_msg(const msg_t &msg);
    // Send the next message in the queue
    void send_pending();
    // Send a pulse to the garden
    // Currently using it for when a sensor is triggered and we want the rest 
    // of the garden to respond
    void send_pulse(uint32_t color, uint8_t fade, uint16_t spread, uint32_t delay);
    // Sensor update message
    void send_sensor_msg(uint16_t strip, uint16_t led, uint16_t percent, uint32_t age);
    // A PIR was triggered. Movement detected from afar
    // Currently only sent when we're in a low energy state, and throttled
    void send_pir_triggered(uint16_t pir_index);

    // Consume the next queued message
    // Intended for consumers
    // Returns true if there was a message available and populates recv_msg_t
    // recv_msg_t will have the message type set
    bool recv_pop(recv_msg_t &msg);
    // Periodic status logging
    void log_info();

private:
    void connect();
    bool recv();

    // Identify ourselves after connecting
    void send_ident();

    // Queue up a message to send
    // Message tossed if we're not connected or if the queue is full
    void queue_send_push(const msg_t &msg);

    void queue_recv_pulse(const char *payload, int len) {
        if(len < MSG_PULSE_SIZE) {
            Serial.printf("Pulse message too short: %d\n", len);
            return;
        }
        msg_pulse_t pulse(payload, len);
        mutex_enter_blocking(&mtx);
        msgq_recv.push(recv_msg_t(pulse));
        mutex_exit(&mtx);
    }

    void queue_recv_state(const char *payload, int len) {
        if(len < MSG_STATE_SIZE) {
            Serial.printf("State message too short: %d\n", len);
            return;
        }
        msg_state_t state(payload, len);
        mutex_enter_blocking(&mtx);
        msgq_recv.push(recv_msg_t(state));
        mutex_exit(&mtx);
    }

    void queue_recv_pir(const char *payload, int len) {
        if(len < MSG_PIR_SIZE) {
            Serial.printf("PIR message too short: %d\n", len);
            return;
        }
        msg_pir_t pir(payload, len);
        mutex_enter_blocking(&mtx);
        msgq_recv.push(recv_msg_t(pir));
        mutex_exit(&mtx);
    }

    void queue_recv_sleepy_time() {
        // This is a signal to the garden to go into a low power state
        // We don't do anything here, just stop the connection
        mutex_enter_blocking(&mtx);
        msgq_recv.push(recv_msg_t(PROTO_SLEEPY_TIME));
        mutex_exit(&mtx);
    }
};
