
PROTO_PING = 1
PROTO_PONG = 2
PROTO_LOG = 3
PROTO_SENSOR = 4
PROTO_STATE_UPDATE = 5
PROTO_PULSE = 6
PROTO_IDENT = 7
PROTO_PIR_TRIGGERED = 8

PROTO_VERSION=1

def build_message(msg_type, payload):
    length = len(payload)
    message = (
        msg_type.to_bytes(2, byteorder="big") +
        PROTO_VERSION.to_bytes(2, byteorder="big") +
        length.to_bytes(2, byteorder="big") +
        payload
    )    
    return message

def parse_header(buffer):
    msg_type = int.from_bytes(buffer[0:2], byteorder="big")
    version = int.from_bytes(buffer[2:4], byteorder="big")
    length = int.from_bytes(buffer[4:6], byteorder="big")

    return msg_type, version, length

def build_state_update(index, value):
    payload = (
        index.to_bytes(4, byteorder="big") +
        value.to_bytes(4, byteorder="big")
    )
    return build_message(PROTO_STATE_UPDATE, payload)