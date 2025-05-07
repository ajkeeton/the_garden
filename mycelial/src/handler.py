from proto import *
import socket

class ProtocolHandler:
    def __init__(self, connection, garden):
        self.connection = connection
        self.garden = garden  # Reference to the Garden instance

    def handle_connection(self):
        use_quickack = False
        if hasattr(socket, 'TCP_QUICKACK'):
            use_quickack = True

        buffer = b""
        while True:
            try:
                # Don't delay our ACKs
                # Must be set each time
                # This only works on Linux
                if use_quickack:
                    self.connection.setsockopt(socket.IPPROTO_TCP, socket.TCP_QUICKACK, 1)

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
                print(f"Error handling connection {self.connection.getpeername()}: {e}")
                raise

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
            #index = int.from_bytes(payload[0:4], byteorder='big')
            self.garden.handle_pir_triggered(self.connection, full_msg)
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
        if len(payload) < 10:
            print(f"Invalid sensor payload: {payload}")
            return
        
        #print(f"sensor message: {payload.hex()}")
        
        index, pct, val = parse_sensor_payload(payload)
 
        # Add additional logic to process the sensor data if needed
        self.garden.handle_sensor(self.connection, index, pct, val)