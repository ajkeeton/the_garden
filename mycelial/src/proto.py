
PROTO_PING = 1
PROTO_LOG = 2
PROTO_SENSOR = 3
PROTO_STATE_UPDATE = 4
PROTO_PULSE = 5
PROTO_IDENT = 6
PROTO_VERSION=1

def parse_message(message):
    if len(message) < 6:
        raise ValueError("Message too short to parse")
    
    msg_type = int.from_bytes(message[0:2], byteorder='big')
    version = int.from_bytes(message[2:4], byteorder='big')
    length = int.from_bytes(message[4:6], byteorder='big')
    
    if len(message) < 6 + length:
        raise ValueError("Message length does not match the specified length")
    
    payload = message[6:6 + length]
    return msg_type, version, payload

def build_message(msg_type, payload):
    length = len(payload)
    message = (
        msg_type.to_bytes(2, byteorder="big") +
        PROTO_VERSION.to_bytes(2, byteorder="big") +
        length.to_bytes(2, byteorder="big") +
        payload
    )    
    return message

def handle_ping(payload):
    # Echo the payload back to the client
    print(f"Received {len(payload)} byte ping: {payload}")
    response = build_message(PROTO_PING, payload)
    return response

def handle_log(payload):
    print(f"Received log message with payload: {payload}")
    return None

def handle_sensor(payload):
    print(f"Received sensor data: {payload}")
    return None

def handle_pulse(payload):
    print(f"Recieved pulse: {payload}")
    return None

def handle_ident(payload):
    print(f"Received ident: {payload}")
    return None

def handle_state_update(payload):
    print(f"Handling state update: {payload}")
    return None