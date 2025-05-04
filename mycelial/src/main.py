import socket
import threading
import time
from handler import ProtocolHandler
from zeroconf import ServiceInfo, Zeroconf
from proto import *
from conf import Config
import math

STATE_TIMEOUT = 5
DEF_CONF = "config.yaml"

class State:
    def __init__(self):
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
                # print(f"Removing old update {i}: {self.updates[i]}")
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
        self.ip_to_mac = {}

        self.t_offsets = {} # map of time offsets for each connection
        self.lock = threading.Lock()

        self.wadsworth = {}
        self.wads = {}
        self.venus = {}
        self.squish = {}

        self.state = State()
        self.conf = Config(DEF_CONF)

    def add_connection(self, addr, connection):
        with self.lock:
            self.connections[addr] = connection
            self.t_offsets[addr] = 0 # this will get set when we receive an ident
            print(f"Added connection: {addr}")

    def remove_connection(self, addr):
        with self.lock:
            print(f"Removing connection for {addr}")

            if addr in self.connections:
                del self.connections[addr]
                del self.t_offsets[addr]

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

    def broadcast(self, ignore_me, message):
        with self.lock:
            for addr, conn in self.connections.items():
                if addr == ignore_me:# or addr not in {**self.wadsworth, **self.wads}:
                    continue
                try:
                    conn.sendall(message)
                    print(f"Sent message to {addr}")
                except Exception as e:
                    print(f"Error sending message to {addr}: {e}")

    def generate_messages(self):
        while True:
            time.sleep(10)

            print("Broadcasting ping message to all clients...")
            self.broadcast(None, build_message(PROTO_PING, b"server ping"))

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
            name, mac, millis = payload.split(",")
            name = str(name)
            # my time in millis since starting
            t = int(time.time() * 1000)
            # offset is the difference between my time and theirs
            # this is the time I should use to calculate delays
            self.t_offsets[ip] = t - int(millis)

            print(f"Ident from: {ip}:{port}, millis: {millis}: ", end="")
            self.active_wads += 1
            self.ip_to_mac[ip] = mac

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
                
            # If there's no entry in the config, add it
            self.conf.touch(name, ip, mac)

        except ValueError:
            print(f"Error handling ident: {payload.hex()}")

    def handle_pulse(self, connection, payload):
        ip, _ = connection.getpeername()
        print(f"{ip}: pulsed with: {payload.hex()}")
        try:   
            self.send_delay_by_dist(ip, payload)
        except Exception as e:
            print(f"Error sending pulse to {ip}: {e}")

    def handle_pir_triggered(self, connection, payload):
        # Placeholder for handling PIR triggered messages
        ip, _ = connection.getpeername()
        print(f"{ip}: PIR triggered with payload: {payload}")
        self.send_delay_by_dist(ip, payload)

    def send_delay_by_dist(self, src_ip, payload):
        # start by building a list of targets with their associated delays
        # delayed are calculated by distance
        targets = []
        with self.lock:
            mac = self.ip_to_mac.get(src_ip)
            if mac is None:
                print(f"Can't send pulse from {src_ip}. No coordinates for {mac}")
                return
            
        with self.lock:
            my_coords = self.conf.get_coords(mac)
            for peer, connection in self.connections.items():
                addr, _ = peer
                if addr == src_ip: # or addr not in {**self.wadsworth, **self.wads}:
                    continue

                r_mac = self.ip_to_mac.get(addr)
                r_coords = self.conf.get_coords(r_mac)

                if r_coords is None:
                    print(f"Can't send pulse to {addr}. Missing coordinates for {r_mac}")
                    continue

                dist = (my_coords['X'] - r_coords['X'])**2 + (my_coords['Y'] - r_coords['Y'])**2
                dist = math.sqrt(dist) 
                delay = int(dist * 100) # Assume 100ms per unit distance
                print(f"Source coordinates: {my_coords}, Target coordinates: {r_coords}")
                print(f"Distance: {dist}, Delay: {delay}ms")

                targets.append((addr, connection, delay))

        # The delayed send is implemented by starting a thread that sleeps 
        # until the delay is over
        def sleep_send(conn, addr, delay):
            time.sleep(delay / 1000.0)  # Convert milliseconds to seconds
            print(f"Send delayed pulse to {addr}")
            conn.sendall(payload)

        for addr, connection, delay in targets:
            #print(f"Scheduling delayed pulse to {addr} with delay {delay}ms")
            # Start a new thread for each target
            threading.Thread(target=sleep_send, 
                    args=(connection, addr, delay), daemon=True).start()
 
    def send_one_delayed(self, connection, payload, sender_coords, receiver_coords):
        with self.lock:
            try:
                # Calculate distance and delay
                dist = (sender_coords[0] - receiver_coords[0])**2 + (sender_coords[1] - receiver_coords[1])**2
                delay = int(dist * 100)  # Assume 100ms per unit distance

                # Adjust for time offset
                current = int(time.time() * 1000)
                delay += current - self.t_offsets[connection.getpeername()[0]]

                print(f"Sending delayed pulse to {connection.getpeername()[0]} with delay {delay}ms")

                # Construct payload with delay
                delayed_payload = (
                    PROTO_PULSE.to_bytes(2, byteorder="big") +
                    PROTO_VERSION.to_bytes(2, byteorder="big") +
                    len(payload).to_bytes(2, byteorder="big") +
                    delay.to_bytes(4, byteorder="big") +
                    payload
                )

                # Send the payload
                connection.sendall(delayed_payload)
            except Exception as e:
                print(f"Error sending delayed pulse to {connection.getpeername()[0]}: {e}")

    def send_state_update(self):
        pat = self.state.pattern
        score = self.state.score / self.active_wads if self.active_wads > 0 else 0
        payload = build_state_update(pat, int(score))

        print(f"Broadcasting state update message: {pat} {score}")
        self.broadcast(None, build_message(PROTO_STATE_UPDATE, payload))

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
