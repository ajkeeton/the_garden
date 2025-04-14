import socket
import threading
import time
from handler import ProtocolHandler
from zeroconf import ServiceInfo, Zeroconf

class Garden:
    def __init__(self):
        self.connections = {}
        self.lock = threading.Lock()

    def add_connection(self, addr, connection):
        with self.lock:
            self.connections[addr] = connection
            print(f"Added connection: {addr}")

    def remove_connection(self, addr):
        with self.lock:
            if addr in self.connections:
                del self.connections[addr]
                print(f"Removed connection: {addr}")

    def broadcast_message(self, message):
        with self.lock:
            for addr, connection in self.connections.items():
                try:
                    connection.sendall(message)
                    print(f"Sent message to {addr}")
                except Exception as e:
                    print(f"Error sending message to {addr}: {e}")

    def generate_messages(self):
        while True:
            time.sleep(10)  # Generate messages every 10 seconds
            message = b"Hello from the Garden!"  # Example message
            self.broadcast_message(message)

def handle_client(client_socket, addr, garden):
    print(f"Connection from {addr}")
    garden.add_connection(addr, client_socket)
    try:
        protocol_handler = ProtocolHandler(client_socket)
        protocol_handler.handle_connection()
    except Exception as e:
        print(f"Error handling connection from {addr}: {e}")
    finally:
        garden.remove_connection(addr)
        client_socket.close()
        print(f"Connection with {addr} closed")

def advertise_service(port):
    zeroconf = Zeroconf()
    local_ip = socket.gethostbyname(socket.gethostname())
    print(local_ip)
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

    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.bind((host, port))
    server_socket.listen()

    print(f"Listening for connections on {host}:{port}...")

    # Advertise the service using mDNS
    zeroconf = advertise_service(port)

    garden = Garden()
    gthread = threading.Thread(target=garden.generate_messages)
    gthread.daemon = True
    gthread.start()

    try:
        while True:
            client_socket, addr = server_socket.accept()
            cthread = threading.Thread(target=handle_client, args=(client_socket, addr, garden))
            cthread.daemon = True  # Ensure threads exit when the main program exits
            cthread.start()
    except KeyboardInterrupt:
        print("Shutting down server...")
    finally:
        zeroconf.close()
        server_socket.close()

if __name__ == "__main__":
    main()