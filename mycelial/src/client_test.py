import socket
import time
from zeroconf import Zeroconf, ServiceBrowser, ServiceStateChange
from proto import *
import threading

class ServerDiscovery:
    def __init__(self, service_type="_garden._tcp.local."):
        self.service_type = service_type
        self.server_address = None
        self.server_port = None
        self.zeroconf = Zeroconf()

    def discover(self):
        def on_service_state_change(zeroconf, service_type, name, state_change):
            if state_change == ServiceStateChange.Added:
                info = zeroconf.get_service_info(service_type, name)
                if info:
                    self.server_address = socket.inet_ntoa(info.addresses[0])
                    self.server_port = info.port
                    print(f"Discovered server at {self.server_address}:{self.server_port}")
                    zeroconf.close()

        print("Discovering server using mDNS...")
        browser = ServiceBrowser(self.zeroconf, self.service_type, handlers=[on_service_state_change])

        # Wait for discovery
        try:
            while not self.server_address:
                time.sleep(0.1)  # Avoid busy-waiting
        except KeyboardInterrupt:
            print("Discovery interrupted.")
            self.zeroconf.close()

        return self.server_address, self.server_port

def send_message(sock, msg_type, payload):
    length = len(payload)

    message = (
        msg_type.to_bytes(2, byteorder="big") +
        PROTO_VERSION.to_bytes(2, byteorder="big") +
        length.to_bytes(2, byteorder="big") +
        payload
    )
    
    # print(f"Hexdump of payload: {message.hex()}")
    sock.sendall(message)
    print(f"Sent message: Type={msg_type}, Length={length}, Payload={payload.hex()}")

def send_mock(sock, iteration):
    send_message(sock, PROTO_LOG,
                 payload=f"Test log {iteration}".encode())

    send_message(sock, PROTO_PING, 
                 payload=f"Test ping {iteration}".encode())

    send_mock_sensor(sock)

def send_mock_sensor(sock):
    # Mock sensor message
    index = 42
    value = 100
    payload = (
        index.to_bytes(4, byteorder="big") +
        value.to_bytes(4, byteorder="big")
    )
    send_message(sock, PROTO_SENSOR, payload)

    # Send pulse
    # uint32_t color, uint8_t fade, uint16_t spread, uint32_t delay

    send_mock_pulse(sock)

def send_mock_pulse(sock):
    color = 0xffffff
    fade = 100
    spread = 10
    delay = 10
    payload = (
        color.to_bytes(4, byteorder="big") +
        fade.to_bytes(1, byteorder="big") +
        spread.to_bytes(2, byteorder="big") +
        delay.to_bytes(4, byteorder="big")
    )

    send_message(sock, PROTO_PULSE, payload)

def main():
    discovery = ServerDiscovery()
    server_address, server_port = discovery.discover()

    if not server_address or not server_port:
        print("Failed to discover server.")
        return
        
    # Connect to the server
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        sock.connect((server_address, server_port))
        print(f"Connected to server at {server_address}:{server_port}")

        def read_responses(sock):
            try:
                while True:
                    response = sock.recv(1024)
                    if not response:
                        print("Server closed the connection.")
                        break
                    print(f"Received response: {response}")
            except Exception as e:
                print(f"Error reading from server: {e}")

        # Start a thread to read responses from the server
        rthread = threading.Thread(target=read_responses, args=(sock,), daemon=True)
        rthread.start()

        while True: # for i in range(5):
            # Send a ping message every second
            #send_mock(sock, i)
            send_mock_pulse(sock)
            time.sleep(1)

        # Close the connection
        print("Closing connection.")

if __name__ == "__main__":
    main()