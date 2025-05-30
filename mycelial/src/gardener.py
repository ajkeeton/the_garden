from conf import Config
import math
import threading
import time
from proto import *

DEF_CONF = "config.yaml"
STATE_TIMEOUT = 5

class State:
    def __init__(self):
        self.t_last_update = 0
        self.updates = []
        self.raw_score = 0
        self.pattern = 0
        self.t_last_pattern_change = 0
        self.active_indices = {}
        self.lock = threading.Lock()

    def update_sensor(self, ip, index, score, age):
        self.t_last_update = time.time()
        self.raw_score += score

        with self.lock:
            self.updates += [(ip, score, self.t_last_update)]
            self.active_indices[ip] = self.t_last_update

        print(f"{ip} updated global state with {score} ({index}, {age}). New global raw score={self.raw_score}")
        print(f"Active nodes: {self.active_indices}")
    
    def update_pulse(self, ip):
        self.t_last_update = time.time()
        self.raw_score += 1

        # keep track of who pulsed us, and the last time we received it
        with self.lock:
            self.updates += [(ip, 1, self.t_last_update)]
            self.active_indices[ip] = self.t_last_update

        print(f"{ip} pulsed us. Active: {len(self.active_indices)}")

        if self.raw_score > 2:
            self.pattern = 1
            
    def cleanup(self):
        i = 0
        tnow = time.time()
        while i < len(self.updates):
            with self.lock:
                ip, score, last = self.updates[i]

            # d = tnow - STATE_TIMEOUT
            #print(f"Checking update {i}: {self.updates[i]}. f{d > last}")

            # Remove old updates and subtract from score
            if tnow - STATE_TIMEOUT < last:
                break

            print(f"Removing old update {i}: {self.updates[i]}")
            self.raw_score -= score
            # the same node may have been active more recently
            # the active_indices value always reflects most recent
            with self.lock:
                if ip in self.active_indices and self.active_indices[ip] == last:
                    print(f"Marking {ip} inactive")
                    del self.active_indices[ip]
                self.updates.pop(i)
            i += 1

        if self.raw_score < 2:
            self.pattern = 0

    def get_score(self):
        # scored in triggers per second over some sliding window?
        return len(self.active_indices) # * 1000 / (time.time() - self.t_last_update) if self.t_last_update > 0 else 0

class Garden:
    def __init__(self):
        self.connections = {}
        self.last_response = {} # tracklast response time for each connection so we can timeout stale ones
        self.ip_to_mac = {}
        self.t_offsets = {} # map of time offsets for each connection
        self.last_score = -1 # used so we don't send the same score twice
        self.lock = threading.Lock()

        self.wadsworth = {}
        self.wads = {}
        self.venus = {}
        self.squish = {}

        self.state = State()
        self.conf = Config(DEF_CONF)
        self.ping_timeout = 60 # Timeout in seconds for stale connections

    def wads_active(self):
        with self.lock:
            return len(self.wads) + len(self.wadsworth)
        
    def add_connection(self, addr, connection):
        with self.lock:
            self.connections[addr] = connection
            self.t_offsets[addr] = 0  # This will get set when we receive an IDENT
            self.last_response[addr] = time.time()  # Initialize last response time
            print(f"Added connection: {addr}")

    def _remove_connection(self, addr):
        if addr in self.connections:
            print(f"Closing connection for {addr}")
            self.connections[addr].close()
            del self.connections[addr]
            del self.t_offsets[addr]
            del self.last_response[addr]

        ip = addr[0]
        if ip in self.wadsworth:
            del self.wadsworth[ip]
        if ip in self.wads:
            del self.wads[ip]
        if ip in self.venus:
            del self.venus[ip]
        if ip in self.squish:
            del self.squish[ip]

    def remove_connection(self, addr):
        with self.lock:
            self._remove_connection(addr)

    def cleanup_stale_connections(self):
        """Remove connections that haven't responded to a ping within the timeout."""
        now = time.time()

        with self.lock:
            for addr, last_time in self.last_response.items():
                if now - last_time > self.ping_timeout:
                    print(f"Connection {addr} is stale. Removing it.")
                    self._remove_connection(addr)
                    # Note: intentionally breaking. Will cleanup the rest later
                    break
                
    def handle_pong(self, conn):
        """Update the last response time when a PONG is received."""
        addr = conn.getpeername()
        with self.lock:
            if addr in self.last_response:
                self.last_response[addr] = time.time()
                print(f"Received PONG from {addr}. Updated last response time.")

    def broadcast(self, ignore_me, message):
        with self.lock:
            for addr, conn in self.connections.items():
                if addr == ignore_me:# or addr not in {**self.wadsworth, **self.wads}:
                    continue
                try:
                    conn.sendall(message)
                    # print(f"Sent message to {addr}")
                except Exception as e:
                    print(f"Error sending message to {addr}: {e}")

    def loop(self):
        last_ping = 0
        last_state_update = time.time()

        while True:
            now = time.time()

            # Send periodic pings
            if now - last_ping > 10:
                last_ping = now
                # print("Broadcasting ping to all clients...")
                self.broadcast(None, build_message(PROTO_PING, b"server ping"))
                self.log_state()

            # Clean up stale connections
            self.cleanup_stale_connections()

            # Send state updates
            if now - last_state_update > 1:
                self.send_state_update()
                last_state_update = now

            time.sleep(0.05)  # Sleep to avoid busy waiting

    def log_state(self):
        print(f"Current state of {self.wads_active()}:")
        print(f"- Score: {self.state.get_score()}, Active: {len(self.state.active_indices)}, Raw: {self.state.raw_score}")

        with self.lock:
            print("- Wadsworth:", list(self.wadsworth.keys()))
            print("- Wads:", self.wads.keys())
            print("- Venus:", self.venus.keys())
            print("- Squishies:", self.squish.keys())

    def handle_sensor(self, connection, index, pct, age):
        # ignoring for now, just relying on pulses
        return

        ip, _ = connection.getpeername()
        print(f"{ip}: sent us sensor data: {index}, {pct}, {age}")

        # Update the state with the sensor data
        # Need the lock since it's periodically cleaned in the other thread
        self.state.update_sensor(ip, index, pct, age)

    def handle_ident(self, connection, payload):
        ip, port = connection.getpeername()
        # print(f"Garden received ident. {ip}:{port} is {payload}")
        # Add logic to handle the IDENT message (e.g., register the client)
        # payload should be <name>,<MAC>

        self.last_response[(ip,port)] = time.time()

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

            if mac not in self.ip_to_mac.values():
                print(f"Ident from: {ip}:{port}, millis: {millis}. It's a {name}")
            
            self.ip_to_mac[ip] = mac

            with self.lock:
                if name == "wadsworth":
                    if ip not in self.wadsworth:
                        print(f"Added Wadsworth @ {ip}")
                    self.wadsworth[ip] = connection
                elif name == "wads":
                    if ip not in self.wads:
                        print(f"Added wad @ {ip}")
                    self.wads[ip] = connection
                elif name == "venus":
                    if ip not in self.venus:
                        print(f"Added venus @ {ip}")
                    self.venus[ip] = connection 
                elif name == "squish":
                    self.squish[ip] = connection 
                else:
                    print(f"Unknown node type: {name}")
                    return
                
            # If there's no entry in the config, add it
            self.conf.touch(name, ip, mac)

        except ValueError:
            print(f"Error handling ident: {payload.hex()}")

    def handle_pulse(self, connection, payload):
        ip, _ = connection.getpeername()

        self.state.update_pulse(ip)
        #print(f"{ip}: pulse message: {payload.hex()}")
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
                print(f"Can't send pulse from {src_ip}. Unknown MAC {mac}")
                return
            
        my_coords = self.conf.get_coords(mac)
        if my_coords is None:
            print(f"Can't send pulse from {src_ip}. Missing coordinates for {mac}")
            return
        
        # XXX TODO: reduce the locking
        with self.lock:
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
                delay = int(dist * 50) # Assume 50ms per unit distance
                print(f"Source coordinates: {my_coords}, Target coordinates: {r_coords}")
                print(f"Distance: {dist}, Delay: {delay}ms")

                targets.append((addr, connection, delay))

        # The delayed send is implemented by starting a thread that sleeps 
        # until the delay is over
        def send_after_delay(conn, addr, delay):
            time.sleep(delay / 1000.0)  # Convert milliseconds to seconds
            print(f"Sending delayed message to {addr}")
            conn.sendall(payload)

        for addr, connection, delay in targets:
            if addr == src_ip:
                continue
            #print(f"Scheduling delayed pulse to {addr} with delay {delay}ms")
            # Start a new thread for each target
            threading.Thread(target=send_after_delay, 
                    args=(connection, addr, delay), daemon=True).start()
 
    def _dep_send_one_delayed(self, connection, payload, sender_coords, receiver_coords):
        try:
            with self.lock:
                # Calculate distance and delay
                dist = (sender_coords[0] - receiver_coords[0])**2 + (sender_coords[1] - receiver_coords[1])**2
                delay = int(dist * 100)  # Assume 100ms per unit distance

                # Adjust for time offset
                current = int(time.time() * 1000)
                delay += current - self.t_offsets[connection.getpeername()[0]]

                # print(f"Sending delayed pulse to {connection.getpeername()[0]} with delay {delay}ms")

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
        act = self.wads_active()
        score = self.state.get_score() / act if act > 0 else 0
        score *= 100 # convert to percentage

        self.state.cleanup()

        if score == self.last_score:
            return

        self.last_score = score

        payload = build_state_update(pat, int(score))

        print(f"Broadcasting state update message: Pattern: {pat} Score: {score} (Raw score {self.state.raw_score} out of {act})")
        self.broadcast(None, build_message(PROTO_STATE_UPDATE, payload))
