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

#define MSG_PULSE_SIZE 11
struct __attribute__((__packed__)) msg_pulse_t {
    // How long to wait before starting the pulse. 
    // XXX this is currently handled server side and can be ignored
    uint32_t t_wait = 0; 
    uint32_t color = 0;
    uint8_t fade = 0;
    uint16_t spread = 0;
    uint32_t delay = 0;

    msg_pulse_t(uint32_t c, uint8_t f, uint16_t s, uint32_t d) :
        color(c), fade(f), spread(s), delay(d) {}

    msg_pulse_t(const char *payload, int len) {
        if (len < MSG_PULSE_SIZE)
            return;

        const uint32_t *p32 = reinterpret_cast<const uint32_t *>(payload);
        t_wait = ntohl(p32[0]);
        color = ntohl(p32[1]);

        const uint16_t *p16 = reinterpret_cast<const uint16_t *>(payload + 8);
        fade = payload[8];
        spread = ntohs(p16[1]);
        delay = ntohl(p16[2]);
    }
    uint32_t min_size() {
        return MSG_PULSE_SIZE;
    }
};

#define MSG_STATE_SIZE 8
struct __attribute__((__packed__)) msg_state_t {
    uint32_t state_idx = 0,
             score = 0;

    msg_state_t(const char *payload, int len) {
        if (len < MSG_STATE_SIZE) {
            return;
        }

        const uint32_t *p32 = reinterpret_cast<const uint32_t *>(payload);
        state_idx = ntohl(p32[0]);
        score = ntohl(p32[1]);

        Serial.printf("State: %lu, %lu\n", state_idx, score);
        Serial.print("Payload: ");
        for (int i = 0; i < len; ++i) {
            Serial.printf("%02X ", (unsigned char)payload[i]);
        }
        Serial.println();
    }

    uint32_t min_size() {
        return MSG_STATE_SIZE;
    }
};

#define MSG_PIR_SIZE 2
struct __attribute__((__packed__)) msg_pir_t {
    // How long to wait before starting the pulse. 
    // XXX this is currently handled server side and can be ignored
    uint32_t t_wait = 0;
    uint16_t placeholder = 0;
    msg_pir_t(const char *payload, int len) {
        if (len < 2)
            return;
            const uint32_t *p32 = reinterpret_cast<const uint32_t *>(payload);
            t_wait = ntohl(p32[0]);

            const uint16_t *p16 = reinterpret_cast<const uint16_t *>(payload + 4);
            placeholder = ntohs(p16[0]);
    }
};

struct recv_msg_t {
    uint16_t type = 0;

    union {
        msg_pulse_t pulse;
        msg_state_t state;
        msg_pir_t pir;
    }; 

    recv_msg_t() {}
    recv_msg_t(const msg_pulse_t &p) : type(PROTO_PULSE), pulse(p) {}
    recv_msg_t(const msg_state_t &s) : type(PROTO_STATE_UPDATE), state(s) {}
    recv_msg_t(const msg_pir_t &s) : type(PROTO_PIR_TRIGGERED), pir(s) {}
};

struct msg_t {
    uint16_t full_len = 0; // full length of the message including header
    char buf[PROTO_HEADER_SIZE + PROTO_MAX_PAYLOAD];

    // Pulse
    msg_t(uint32_t color, uint8_t fade, uint16_t spread, uint32_t delay) {
        msg_pulse_t pulse(htonl(color), fade, htons(spread), htonl(delay));
        memcpy(buf + PROTO_HEADER_SIZE, &pulse, MSG_PULSE_SIZE);
        init_header(PROTO_PULSE, MSG_PULSE_SIZE);
    }

    // Sensor update message
    msg_t(uint16_t strip, uint16_t led, uint16_t percent, uint32_t age) {
        // The strip and led combined are used as an unique ID for the sensor
        uint16_t *buf16 = reinterpret_cast<uint16_t *>(buf + PROTO_HEADER_SIZE);
        buf16[0] = htons(strip);
        buf16[1] = htons(led);
        buf16[2] = htons(percent);

        uint32_t *buf32 = reinterpret_cast<uint32_t *>(buf + PROTO_HEADER_SIZE + 6);
        buf32[0] = htonl(age);

        init_header(PROTO_SENSOR, 10);
    }

    // IDENT
    msg_t(const char *name, const char *mac) {
        int len = snprintf(buf + PROTO_HEADER_SIZE, 
                          sizeof(buf) - PROTO_HEADER_SIZE, 
            "%s,%s,%lu", name, mac, millis());
        init_header(PROTO_IDENT, len);
    }

    // PIR message
    msg_t(uint16_t pir_index) {
        // write the index to the payload directly
        *(uint32_t*)(buf) = 0;
        buf[4] = pir_index & 0xFF;
        buf[5] = (pir_index >> 8) & 0xFF;
        init_header(PROTO_PIR_TRIGGERED, 2);
    }

    void init_header(uint16_t t, int pay_len) {
        buf[0] = (t >> 8) & 0xFF;
        buf[1] = t & 0xFF;
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
    std::queue<msg_t> msgq_send;
    std::queue<recv_msg_t> msgq_recv;

    uint32_t retry_in = 0,
             last_log = 0,
             last_retry = 0,
             // With the WiFi going up and down we sometimes don't realize 
             // the connection is gone. Now we expect periodic pings from the 
             // server or else we close the connection and reconnect
             t_last_server_contact = 0;
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
};
