import socket
import threading
import time
from handler import ProtocolHandler
from zeroconf import ServiceInfo, Zeroconf
from proto import *

STATE_TIMEOUT = 5

class State:
    def __init__(self):
        #self.data = {}
        self.t_last_update = 0
        self.updates = []
        self.score = 0
        self.pattern = 0
        self.active_indices = {}

    def update(self, ip, index, score):
        self.t_last_update = time.time()
        self.cleanup()

        self.updates += [(ip, score, self.t_last_update)]
        self.score += score
        self.active_indices[ip] = self.t_last_update

        print(f"{ip} updated global state with {score}. New global score={self.score}")
        print(f"Active nodes: {self.active_indices}")
        
    def cleanup(self):
        i = 0
        tnow = time.time()
        while i < len(self.updates):
            ip, score, last = self.updates[i]

            # d = tnow - STATE_TIMEOUT
            #print(f"Checking update {i}: {self.updates[i]}. f{d > last}")

            # Remove old updates and subtract from score
            if tnow - STATE_TIMEOUT > last:
                print(f"Removing old update {i}: {self.updates[i]}")
                self.score -= score
                # the same node may have been active more recently
                # the active_indices value always reflects most recent
                if ip in self.active_indices and self.active_indices[ip] == last:
                    del self.active_indices[ip]
                self.updates.pop(i)
            i += 1

    def get_score(self):
        # scored in triggers per second over some sliding window?
        return self.score

class Garden:
    def __init__(self):
        self.active_wads = 0
        self.connections = {}
        self.lock = threading.Lock()

        self.wadsworth = {}
        self.wads = {}
        self.venus = {}
        self.squish = {}

        self.state = State()

    def add_connection(self, addr, connection):
        with self.lock:
            self.connections[addr] = connection
            print(f"Added connection: {addr}")

    def remove_connection(self, addr):
        with self.lock:
            print(f"Removing connection for {addr}")

            if addr in self.connections:
                del self.connections[addr]
            ip, _ = addr
            if ip in self.wadsworth:
                self.active_wads -= 1
                del self.wadsworth[ip]
            if ip in self.wads:
                self.active_wads -= 1
                del self.wads[ip]
            if ip in self.venus:
                self.active_wads -= 1
                del self.venus[ip]
            if ip in self.squish:
                self.active_wads -= 1
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

            print("Current state:")
            with self.lock:
                self.state.cleanup()
                print(f"- Score: {self.state.get_score()}, Active: {len(self.state.active_indices)}")
                print("- Wadsworth:", list(self.wadsworth.keys()))
                print("- Wads:", self.wads.keys())
                print("- Venus:", self.venus.keys())
                print("- Squishies:", self.squish.keys())

            if self.state.score > 0:
                print("Sending state update")
                self.send_state_update()

    def send_state_update(self):
        pat = self.state.pattern
        score = self.state.score / self.active_wads if self.active_wads > 0 else 0
        
        score = int(score)
        payload = (
            pat.to_bytes(4, byteorder="big") +
            score.to_bytes(4, byteorder="big")
        )
        
        print(f"Broadcasting state update message: {pat} {score}")
        self.broadcast(build_message(PROTO_STATE_UPDATE, payload))

    def handle_sensor(self, connection, index, score):
        ip, _ = connection.getpeername()
        print(f"{ip}: sent us sensor data: {index}, {score}")

        with self.lock:
            # Update the state with the sensor data
            # Need the lock since it's periodically cleaned in the other thread
            self.state.update(ip, index, score)

    def handle_ident(self, connection, payload):
        ip, port = connection.getpeername()
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
            self.active_wads += 1
            with self.lock:
                if name == "wadsworth":
                    print(f"It's a Wadsworth! MAC: {mac}")
                    self.wadsworth[ip] = connection
                    #self.whatwhere[ip] = self.wadsworth
                elif name == "wads":
                    print("It's one of accessory wads!")
                    self.wads[ip] = connection
                elif name == "venus":
                    print("It's the hippy trap!")
                    self.venus[ip] = connection 
                elif name == "squish":
                    print("Oooo it's all squishy!")
                    self.squish[ip] = connection 
                else:
                    self.active_wads -= 1
                    print(f"Unknown node type: {name}")
                    return
        except ValueError:
            print(f"Error splitting payload: {payload}")

        #self.nodes[connection.getpeername()] = payload

    def handle_pulse(self, connection, payload):
        src_ip, srcport = connection.getpeername()

        print(f"{src_ip}: pulsed with: {payload.hex()}")

        # TODO: look up coords to determine timing
       
        with self.lock:
            # forward pulse to all connections
            for peer, c in self.connections.items():
                addr, port = peer
                if addr == src_ip:
                    continue
                if addr in self.wadsworth or addr in self.wads:
                    print(f"Forwarding pulse to {addr}")
                    try:
                        c.sendall(payload)
                    except Exception as e:
                        print(f"Error sending pulse to {addr}: {e}")

    def handle_pir_triggered(self, connection, payload):
        # Placeholder for handling PIR triggered messages
        ip, _ = connection.getpeername()
        print(f"{ip}: PIR triggered with payload: {payload}")
        print("TODO: send to rest of garden with approp delays")

        with self.lock:
            # forward pir signal to all connections
            for peer, c in self.connections.items():
                addr, port = peer
                if addr == ip:
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
    # Allow address reuse
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
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
