import socket
import time
from zeroconf import Zeroconf, ServiceBrowser, ServiceStateChange

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

def send_message(sock, msg_type, version, payload):
    length = len(payload)
    message = (
        msg_type.to_bytes(2, byteorder="big") +
        version.to_bytes(2, byteorder="big") +
        length.to_bytes(2, byteorder="big") +
        payload
    )
    sock.sendall(message)
    print(f"Sent message: Type={msg_type}, Version={version}, Length={length}, Payload={payload}")

def main():
    discovery = ServerDiscovery()
    server_address, server_port = discovery.discover()

    if not server_address or not server_port:
        print("Failed to discover server.")
        return

    # Connect to the server
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.connect((server_address, server_port))
        print(f"Connected to server at {server_address}:{server_port}")

        # Send a mock ping message (Type=1)
        send_message(sock, msg_type=1, version=1, payload=b"PingPayload")

        # Send a mock log message (Type=2)
        send_message(sock, msg_type=2, version=1, payload=b"LogPayload")

        # Send a mock sensor message (Type=3)
        # Example: Index=42, Value=100
        index = 42
        value = 100
        sensor_payload = (
            index.to_bytes(4, byteorder="big") +
            value.to_bytes(4, byteorder="big")
        )
        send_message(sock, msg_type=3, version=1, payload=sensor_payload)

        # Close the connection
        print("Closing connection.")

if __name__ == "__main__":
    main()