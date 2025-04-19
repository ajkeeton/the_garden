#pragma once
const uint16_t PROTO_VERSION = 1,
            PROTO_PING = 1,
            PROTO_LOG = 2,
            PROTO_SENSOR = 3,
            PROTO_STATE_UPDATE = 4,
            PROTO_PULSE = 5,
            PROTO_IDENT = 6;

#define PROTO_MAX_PAYLOAD 512
#define PROTO_HEADER_SIZE 6 // 2 bytes for type, 2 for version, 2 for length