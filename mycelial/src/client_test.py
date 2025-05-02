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

        try:
            while not self.server_address:
                time.sleep(0.1) 
        except KeyboardInterrupt:
            print("Discovery interrupted.")
            self.zeroconf.close()

        return self.server_address, self.server_port

def send_message(sock, msg_type, payload):
    msg = build_message(msg_type, payload)
    sock.sendall(msg)
    print(f"Sent: Type={msg_type}, Payload={payload.hex()}")

def send_mock(sock, iteration):
    send_message(sock, PROTO_LOG,
                 payload=f"Test log {iteration}".encode())

    send_message(sock, PROTO_PING, 
                 payload=f"Test ping {iteration}".encode())

    send_mock_sensor(sock)

def send_ident(sock, name):
    payload = f"{name},MAC".encode()
    send_message(sock, PROTO_IDENT, payload)    

def send_mock_sensor(sock):
    # Mock sensor message
    index = 42
    value = 100
    payload = (
        index.to_bytes(4, byteorder="big") +
        value.to_bytes(4, byteorder="big")
    )
    send_message(sock, PROTO_SENSOR, payload)

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

def send_mock_pir(sock):
    # Mock PIR triggered message
    index = 1
    payload = (
        index.to_bytes(2, byteorder="big")
    )
    send_message(sock, PROTO_PIR_TRIGGERED, payload)

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
                buffer = b""
                while True:
                    response = sock.recv(1024)
                    if not response:
                        print("Server closed the connection.")
                        break

                    buffer += response

                    while len(buffer) >= 6:  # Minimum size for a message header
                        msg_type, version, length = parse_header(buffer)
                        if len(buffer) < 6 + length:
                            # Wait for more data if the full message hasn't arrived yet
                            break

                        payload = buffer[6:6 + length]
                        print(f"Parsed response. Type: {msg_type}, Version: {version}, Payload: 0x{payload.hex()}")
                        buffer = buffer[6 + length:]

            except Exception as e:
                print(f"Error reading from server: {e}")

        # Start a thread to read responses from the server
        rthread = threading.Thread(target=read_responses, args=(sock,), daemon=True)
        rthread.start()

        send_ident(sock, "wads")

        while True:
            send_mock(sock, 1)
            #send_mock_pulse(sock)
            send_mock_pir(sock)
            time.sleep(1)

        print("Closing")

if __name__ == "__main__":
    main()