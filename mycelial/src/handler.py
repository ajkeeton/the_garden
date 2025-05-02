from proto import *
import socket

class ProtocolHandler:
    def __init__(self, connection, garden):
        self.connection = connection
        self.garden = garden  # Reference to the Garden instance

    def handle_connection(self):
        buffer = b""
        while True:
            try:
                # Don't delay our ACKs
                # This only works on Linux
                try:
                    if hasattr(socket, 'TCP_QUICKACK'):
                        self.connection.setsockopt(socket.IPPROTO_TCP, socket.TCP_QUICKACK, 1)
                except Exception as e:
                    print(f"Error setting TCP_QUICKACK: {e}")

                data = self.connection.recv(1024)
                if not data:
                    break
                buffer += data

                # Process all complete messages in the buffer
                while len(buffer) >= 6:  # Minimum size for a message header
                    msg_type, version, length = parse_header(buffer)

                    if len(buffer) < 6 + length:
                        # Wait for more data if the full message hasn't arrived yet
                        break

                    payload = buffer[6:6 + length]
                    self.process_message(msg_type, version, buffer[0:6+length], payload)
                    buffer = buffer[6 + length:]  # Remove the processed message from the buffer
            except Exception as e:
                print(f"Error handling connection: {e}")
                break

    def process_message(self, msg_type, version, full_msg, payload):
        #print(f"Parsing {len(payload)} byte message")
        if msg_type == PROTO_PING:
            self.handle_ping(payload)
        elif msg_type == PROTO_PONG:
            print("Received PONG")
        elif msg_type == PROTO_LOG:
            self.handle_log(payload)
        elif msg_type == PROTO_STATE_UPDATE:
            print(f"Ignoring state update message with payload: {payload.hex()}")
        elif msg_type == PROTO_PULSE:
            self.garden.handle_pulse(self.connection, full_msg)
        elif msg_type == PROTO_IDENT:
            self.garden.handle_ident(self.connection, payload)
        elif msg_type == PROTO_SENSOR:
            self.handle_sensor(payload)
        elif msg_type == PROTO_PIR_TRIGGERED:
            # Placeholder for handling PIR triggered messages
            index = int.from_bytes(payload[0:4], byteorder='big')
            self.garden.handle_pir_triggered(self.connection, index)
        else:
            # Placeholder for other message types
            print(f"Received message - Type: {msg_type}, Version: {version}, Payload: {payload}")

    def handle_ping(self, payload):
        ip, _ = self.connection.getpeername()

        # Echo the payload back to the client
        print(f"{ip}: {len(payload)} byte ping message with: {payload}")
        resp = build_message(PROTO_PONG, payload)
        self.connection.sendall(resp)

    def handle_log(self, payload):
        # Placeholder for handling log messages
        ip ,_ = self.connection.getpeername()

        print(f"{ip}: Handling log message: {payload}")

    def handle_sensor(self, payload):
        if len(payload) < 8:
            print(f"Invalid sensor payload: {payload}")
            return
        
        # Extract index (4 bytes) and value (4 bytes)
        index = int.from_bytes(payload[0:4], byteorder='big')
        value = int.from_bytes(payload[4:8], byteorder='big')
        
        # Add additional logic to process the sensor data if needed
        self.garden.handle_sensor(self.connection, index, value)
