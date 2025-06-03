#pragma once
const uint16_t PROTO_VERSION = 1,
            PROTO_PING = 1,
            PROTO_PONG = 2,
            PROTO_LOG = 3,
            PROTO_SENSOR = 4,
            PROTO_STATE_UPDATE = 5,
            PROTO_PULSE = 6,
            PROTO_IDENT = 7,
            PROTO_PIR_TRIGGERED = 8,
            PROTO_SLEEPY_TIME = 9;

#define PROTO_MAX_PAYLOAD 512
#define PROTO_HEADER_SIZE 6 // 2 bytes for type, 2 for version, 2 for length

#define MSG_STATE_SIZE 8
#define MSG_PIR_SIZE 6
#define MSG_SENSE_SIZE 10
#define MSG_PULSE_SIZE 15

struct __attribute__((__packed__)) msg_pulse_t {
    // How long to wait before starting the pulse. 
    // XXX this is currently handled server side and can be ignored
    uint32_t t_wait = 0; 
    uint32_t color = 0;
    uint8_t fade = 0;
    uint16_t spread = 0;
    uint32_t delay = 0;

    msg_pulse_t(uint32_t c, uint8_t f, uint16_t s, uint32_t d) :
        color(htonl(c)), fade(f), spread(htons(s)), delay(htonl(d)) {}

    msg_pulse_t(const char *payload, int len) {
        if (len < MSG_PULSE_SIZE)
            return;

        const uint32_t *p32 = reinterpret_cast<const uint32_t *>(payload);
        t_wait = ntohl(p32[0]);
        color = ntohl(p32[1]);

        fade = payload[8];
        const uint16_t *p16 = reinterpret_cast<const uint16_t *>(payload + 9);
        spread = ntohs(p16[0]);

        p32 = reinterpret_cast<const uint32_t *>(payload + 11);
        delay = ntohl(p32[0]);

        Serial.printf("Parsed pulse: %lu, %u, %u, %lu\n", color, fade, spread, delay);
        //Serial.print("Payload hexdump: ");
        //for (int i = 0; i < len; ++i) {
        //    Serial.printf("%02X ", static_cast<unsigned char>(payload[i]));
        //}
        //Serial.println();
    }
    uint32_t min_size() {
        return MSG_PULSE_SIZE;
    }
};

struct __attribute__((__packed__)) msg_state_t {
    uint32_t pattern_idx = 0,
             score = 0;

    msg_state_t(const char *payload, int len) {
        if (len < MSG_STATE_SIZE) {
            return;
        }

        const uint32_t *p32 = reinterpret_cast<const uint32_t *>(payload);
        pattern_idx = ntohl(p32[0]);
        score = ntohl(p32[1]);

        #if 0
        Serial.printf("State: %lu, %lu\n", state_idx, score);
        Serial.print("Payload: ");
        for (int i = 0; i < len; ++i) {
            Serial.printf("%02X ", (unsigned char)payload[i]);
        }
        Serial.println();
        #endif
    }

    uint32_t min_size() {
        return MSG_STATE_SIZE;
    }

    msg_state_t() {}
};

struct __attribute__((__packed__)) msg_pir_t {
    // How long to wait before starting the pulse. 
    // XXX this is currently handled server side and can be ignored
    uint32_t t_wait = 0;
    uint16_t placeholder = 0;
    msg_pir_t(const char *payload, int len) {
        if (len < MSG_PIR_SIZE)
            return;
        const uint32_t *p32 = reinterpret_cast<const uint32_t *>(payload);
        t_wait = ntohl(p32[0]);

        const uint16_t *p16 = reinterpret_cast<const uint16_t *>(payload + 4);
        placeholder = ntohs(p16[0]);
    }
};

struct msg_t {
    uint16_t full_len = 0; // full length of the message including header
    char buf[PROTO_HEADER_SIZE + PROTO_MAX_PAYLOAD];

    // Pulse
    msg_t(uint32_t color, uint8_t fade, uint16_t spread, uint32_t delay) {
        msg_pulse_t pulse(color, fade, spread, delay);
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

        init_header(PROTO_SENSOR, MSG_SENSE_SIZE);
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
        //*(uint32_t*)(buf) = 0;
        //buf[4] = pir_index & 0xFF;
        //buf[5] = (pir_index >> 8) & 0xFF;
        uint32_t *buf32 = reinterpret_cast<uint32_t *>(buf + PROTO_HEADER_SIZE);

        buf32[0] = htonl(pir_index);
        buf32[1] = 0; // placeholder

        init_header(PROTO_PIR_TRIGGERED, MSG_PIR_SIZE);
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
