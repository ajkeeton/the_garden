import socket
import time
from zeroconf import Zeroconf, ServiceBrowser, ServiceStateChange
import sys
import os
import threading

# Add the parent directory to sys.path to allow relative import
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
from proto import *
import sys

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

def send_ident(sock, name, mac):
    # get time since start in milliseconds
    start_time = int(time.time() * 1000)
    #payload = start_time.to_bytes(4, byteorder="big")
    payload = f"{name},{mac},{start_time}".encode()
    send_message(sock, PROTO_IDENT, payload)    

def send_sleepy_time(sock):
    print("Sending sleepy time override message")
    payload = "".encode()
    send_message(sock, PROTO_SLEEPY_TIME, payload)  

def main():
    mac = "MOCK-MAC"

    if len(sys.argv) == 2:
        mac = sys.argv[1]
        print(f"Using MAC", mac)

    discovery = ServerDiscovery()
    server_address, server_port = discovery.discover()

    if not server_address or not server_port:
        print("Failed to discover server.")
        return  
    
    do_client(server_address, server_port, mac)

def do_client(server_address, server_port, mac):   
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

            except OSError as e:
                #print("Socket error: Bad file descriptor, likely due to shutdown.")
                pass
            except Exception as e:
                print(f"Error reading from server: {e}")

        # Start a thread to read responses from the server
        rthread = threading.Thread(target=read_responses, args=(sock,), daemon=True)
        rthread.start()

        send_ident(sock, "wads", mac)
        send_sleepy_time(sock)

        try:
            sock.shutdown(socket.SHUT_RDWR)
        except Exception:
            pass  # Socket may already be closed

    # Wait for the read thread to finish
    rthread.join(timeout=2)
    print("Client thread finished.")

if __name__ == "__main__":
    main()