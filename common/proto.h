#pragma once
const uint16_t PROTO_VERSION = 1,
            PROTO_PING = 1,
            PROTO_PONG = 2,
            PROTO_LOG = 3,
            PROTO_SENSOR = 4,
            PROTO_STATE_UPDATE = 5,
            PROTO_PULSE = 6,
            PROTO_IDENT = 7,
            PROTO_PIR_TRIGGERED = 8;

#define PROTO_MAX_PAYLOAD 512
#define PROTO_HEADER_SIZE 6 // 2 bytes for type, 2 for version, 2 for length