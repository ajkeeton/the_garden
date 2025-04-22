import socket
import threading
import time
from handler import ProtocolHandler
from zeroconf import ServiceInfo, Zeroconf
from proto import *

class Garden:
    def __init__(self):
        self.connections = {}
        self.lock = threading.Lock()

        self.wadsworth = {}
        self.wads = {}
        self.venus = {}
        self.squish = {}

    def add_connection(self, addr, connection):
        with self.lock:
            self.connections[addr] = connection
            print(f"Added connection: {addr}")

    def remove_connection(self, addr):
        with self.lock:
            print(f"Removing connection for {addr}")

            if addr in self.connections:
                del self.connections[addr]
            ip, port = addr
            if ip in self.wadsworth:
                del self.wadsworth[ip]
            if ip in self.wads:
                del self.wads[ip]
            if ip in self.venus:
                del self.venus[ip]
            if ip in self.squish:
                del self.squish[ip]

    def broadcast(self, message):
        with self.lock:
            for addr, connection in self.connections.items():
                try:
                    connection.sendall(message)
                    print(f"Sent message to {addr}")
                except Exception as e:
                    print(f"Error sending message to {addr}: {e}")

    def generate_messages(self):
        while True:
            time.sleep(10)  
            print("Broadcasting ping message to all clients...")
            self.broadcast(build_message(PROTO_PING, b"server ping"))

            print("Current state of connections:")
            print("- Wadsworth:", list(self.wadsworth.keys()))
            print("- Wads:", self.wads.keys())
            print("- Venus:", self.venus.keys())
            print("- Squishies:", self.squish.keys())

    def handle_ident(self, connection, payload):
        ip, port = connection.getpeername()
        # Process the IDENT message payload
        # print(f"Garden received ident. {ip}:{port} is {payload}")
        # Add logic to handle the IDENT message (e.g., register the client)

        # payload should be <name>,<MAC>

        try:
            payload = payload.decode("utf-8")
        except UnicodeDecodeError:
            print(f"Error decoding payload: {payload}")
            return
        
        try:
            name, mac = payload.split(",")
            name = str(name)
            print(f"Ident from: {ip}:{port}, ", end="")
            if name == "wadsworth":
                print("It's a Wadsworth!")
                self.wadsworth[ip] = connection 
            elif name == "accessory":
                print("It's one of accessory wads!")
                self.wads[ip] = connection
            elif name == "venus":
                print("It's the hippy trap!")
                self.venus[ip] = connection 
            elif name == "squish":
                print("Oooo it's all squishy!")
                self.squish[ip] = connection 
            else:
                print(f"Unknown node type: {name}")
                return
        except ValueError:
            print(f"Error splitting payload: {payload}")

        #self.nodes[connection.getpeername()] = payload

    def handle_pulse(self, connection, payload):
        src_ip, srcport = connection.getpeername()

        # Process the IDENT message payload
        print(f"Garden received pulse from {connection.getpeername()} with payload: {payload.hex()}")

        # TODO: look up coords to determine timing
       
        # forward pulse to all connections
        for peer, c in self.connections.items():
            addr, port = peer
            #print("Checking", addr)
            if addr == src_ip:
                #print("same source & dest, skipping")
                continue
            if addr in self.wadsworth or addr in self.wads:
                print(f"Forwarding pulse to {addr}")
                try:
                    c.sendall(payload)
                except Exception as e:
                    print(f"Error sending pulse to {addr}: {e}")
           
def handle_client(csock, addr, garden):
    print(f"Connection from {addr}")
    garden.add_connection(addr, csock)
    try:
        protocol_handler = ProtocolHandler(csock, garden) 
        protocol_handler.handle_connection()
    except Exception as e:
        print(f"Error handling connection from {addr}: {e}")
    finally:
        garden.remove_connection(addr)
        csock.close()
        print(f"Connection with {addr} closed")

def advertise_service(port):
    zeroconf = Zeroconf()

    # Get the actual local IP address (make sure it doesn't return localhost)
    # I think this works even when we can't access 8.8.8.8
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.connect(("8.8.8.8", 80))  # Connect to a public IP to determine the local IP
        local_ip = s.getsockname()[0]

    print(f"Advertising service on IP: {local_ip}")
    service_info = ServiceInfo(
        "_garden._tcp.local.",
        "GardenServer._garden._tcp.local.",
        addresses=[socket.inet_aton(local_ip)],
        port=port,
        properties={"description": "Garden IoT Server"},
        server="garden.local.",
    )
    zeroconf.register_service(service_info)
    print("mDNS service advertised as 'GardenServer._garden._tcp.local.'")
    return zeroconf

def main():
    host = '0.0.0.0'
    port = 7777

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)  # Allow address reuse
    sock.bind((host, port))
    sock.listen()

    print(f"Listening for connections on {host}:{port}...")

    # Advertise the service using mDNS
    zeroconf = advertise_service(port)

    garden = Garden()
    gthread = threading.Thread(target=garden.generate_messages)
    gthread.daemon = True
    gthread.start()

    try:
        while True:
            csock, addr = sock.accept()
            cthread = threading.Thread(target=handle_client, args=(csock, addr, garden))
            cthread.daemon = True  # Ensure threads exit when the main program exits
            cthread.start()
    except KeyboardInterrupt:
        print("Shutting down server...")
    finally:
        zeroconf.close()
        sock.close()

if __name__ == "__main__":
    main()