from proto import *

class ProtocolHandler:
    def __init__(self, connection):
        self.connection = connection

    def parse_message(self, message):
        if len(message) < 6:
            raise ValueError("Message too short to parse")
        
        msg_type = int.from_bytes(message[0:2], byteorder='big')
        version = int.from_bytes(message[2:4], byteorder='big')
        length = int.from_bytes(message[4:6], byteorder='big')
        
        if len(message) < 6 + length:
            raise ValueError("Message length does not match the specified length")
        
        payload = message[6:6 + length]
        return msg_type, version, payload

    def handle_connection(self):
        while True:
            message = self.connection.recv(1024)
            if not message:
                break
            
            try:
                msg_type, version, payload = self.parse_message(message)
                self.process_message(msg_type, version, payload)
            except ValueError as e:
                print(f"Error parsing message: {e}")

    def process_message(self, msg_type, version, payload):
        # Handle ping messages (type 1)
        if msg_type == PROTO_PING:
            self.handle_ping(payload)
        elif msg_type == PROTO_LOG:
            self.handle_log(payload)
        elif msg_type == PROTO_SENSOR:
            self.handle_sensor(payload)
        elif msg_type == PROTO_STATE_UPDATE:
            print(f"Handling state update message with payload: {payload}")
        elif msg_type == PROTO_PULSE:
            print(f"Handling pulse message with payload: {payload}")
        else:
            # Placeholder for other message types
            print(f"Received message - Type: {msg_type}, Version: {version}, Payload: {payload}")

    def handle_ping(self, payload):
        # Echo the payload back to the client
        print(f"Handling {len(payload)} byte ping message with payload: {payload}")
        response = (1).to_bytes(2, byteorder='big')  # Message type: 1 (ping)
        response += (1).to_bytes(2, byteorder='big')  # Version: 1
        response += len(payload).to_bytes(2, byteorder='big')  # Length of payload
        response += payload  # Payload
        self.connection.sendall(response)

    def handle_log(self, payload):
        # Placeholder for handling log messages
        print(f"Handling log message with payload: {payload}")

    def handle_sensor(self, payload):
        if len(payload) < 4:
            print(f"Invalid sensor payload: {payload}")
            return
        
        # Extract index (4 bytes) and value (4 bytes)
        index = int.from_bytes(payload[0:2], byteorder='big')
        value = int.from_bytes(payload[2:4], byteorder='big')
        
        print(f"Handling sensor message - Index: {index}, Value: {value}")
        # Add additional logic to process the sensor data if needed