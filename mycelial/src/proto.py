
PROTO_PING = 1
PROTO_PONG = 2
PROTO_LOG = 3
PROTO_SENSOR = 4
PROTO_STATE_UPDATE = 5
PROTO_PULSE = 6
PROTO_IDENT = 7
PROTO_PIR_TRIGGERED = 8
PROTO_SLEEPY_TIME = 9
PROTO_OVERRIDE_STATE = 10 # tell the gardener what state we're in, mostly for one-off control scripts
PROTO_FORCE_SLEEP = 11
PROTO_VERSION=1

def build_message(msg_type, payload):
    length = len(payload)
    message = (
        msg_type.to_bytes(2, byteorder="big") +
        PROTO_VERSION.to_bytes(2, byteorder="big") +
        length.to_bytes(2, byteorder="big") +
        payload
    )

    print(message)
    return message

def parse_header(buffer):
    msg_type = int.from_bytes(buffer[0:2], byteorder="big")
    version = int.from_bytes(buffer[2:4], byteorder="big")
    length = int.from_bytes(buffer[4:6], byteorder="big")

    return msg_type, version, length

def parse_sensor_payload(payload):
    if len(payload) < 10:
        print(f"Invalid sensor payload: {payload}")
        return None

    index = int.from_bytes(payload[0:4], byteorder='big')
    pct = int.from_bytes(payload[4:6], byteorder='big')
    value = int.from_bytes(payload[6:10], byteorder='big')

    #print(f"Parsed sensor payload: index={index}, value={value}")
    return index, pct, value

def parse_pulse_payload(payload):
    if len(payload) < 12:
        print(f"Invalid pulse payload: {payload}")
        return None

    wait = int.from_bytes(payload[0:4], byteorder='big')
    color = int.from_bytes(payload[4:8], byteorder='big')
    fade = int.from_bytes(payload[8:9], byteorder='big')
    spread = int.from_bytes(payload[9:11], byteorder='big')
    delay = int.from_bytes(payload[11:15], byteorder='big')

    #print(f"Parsed pulse payload: wait={wait}, color={color}, fade={fade}, spread={spread}, delay={delay}")
    return wait, color, fade, spread, delay

def build_state_update(index, value):
    payload = (
        index.to_bytes(4, byteorder="big") +
        value.to_bytes(4, byteorder="big")
    )

    print(f"Building state update message with index {index} and value {value}")
    # print(f"Payload: {payload.hex()}")

    return payload