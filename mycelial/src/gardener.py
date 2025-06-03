from conf import Config
import math
import threading
import time
from proto import *

DEF_CONF = "config.yaml"
TIMEOUT_STATE_UPDATE = 10
TIMEOUT_FORCED_STATE_UPDATE_MESSAGE = 30
TIMEOUT_BROADCAST_PING = 20
TIMEOUT_DAYTIME_DETECTION = 10 
MIN_DAYTIME_THOLD = 500 # average raw values over this are considered daytime

class State:
    def __init__(self):
        self.t_last_update = 0
        self.updates = []
        self.avg_raw_garden_score = 0
        self.pattern = 0
        self.t_last_pattern_change = 0
        self.sensors = {}
        self.active_sensors = {}
        self.score = 0
        self.lock = threading.Lock()

        self.daytime = False
        self.t_start_sun_detection = 0  # Start time for sun detection

    def update_sensor(self, ip, index, raw):
        self.t_last_update = time.time()

        if raw < 1:
            return

        with self.lock:
            self.updates += [(ip, raw, self.t_last_update)]

            if ip not in self.sensors:
                self.sensors[ip] = {}
            self.sensors[ip][index] = raw

        # XXX Future: only do this calc periodically
        self.calc_raw()

        #print(f"{ip} updated global state with {raw} ({index}). New global raw score={self.avg_raw_garden_score}")
         #print(f"Sensors: {self.sensors}")

        if self.avg_raw_garden_score > MIN_DAYTIME_THOLD:
            if not self.daytime:
                if not self.t_start_sun_detection:
                    self.t_start_sun_detection = time.time()
                return

            if self.t_start_sun_detection and (time.time() - self.t_start_sun_detection > TIMEOUT_DAYTIME_DETECTION):
                self.set_daytime()
        else:
            if self.daytime:
                if not self.t_start_sun_detection:
                    self.t_start_sun_detection = time.time()
                elif (time.time() - self.t_start_sun_detection > TIMEOUT_DAYTIME_DETECTION):
                    self.clear_daytime()
            else:
                self.t_start_sun_detection = 0  # Reset timer if already not daytime

    def set_daytime(self):
        if self.pattern != 0:
            self.t_last_pattern_change = time.time()
            self.pattern = 0
        if not self.daytime:
            print(f"Setting daytime mode. Raw score: {self.avg_raw_garden_score}")
            self.daytime = True 
            self.t_start_sun_detection = 0

    def clear_daytime(self):
        print(f"Clearing daytime mode. Raw score: {self.avg_raw_garden_score}")

        self.daytime = False
        self.t_start_sun_detection = 0

    def calc_raw(self):
        """Calculate the raw score based on the current updates."""
        self.avg_raw_garden_score = 0
        count = 0
        for sensor_dict in self.sensors.values():
            for raw in sensor_dict.values():
                self.avg_raw_garden_score += raw
                count += 1
        self.avg_raw_garden_score /= count if count else 1  # Avoid division by zero

    def update_pulse(self, ip):
        self.t_last_update = time.time()

        # keep track of who pulsed us, and the last time we received it
        with self.lock:
            self.updates += [(ip, 1, self.t_last_update)]
            self.active_sensors[ip] = self.t_last_update

        print(f"{ip} pulsed us. Active: {len(self.active_sensors)}")

        # XXX Future: only do this calc periodically
        self.calc_score()

    def calc_score(self):
        """Calculate the score based on active sensors"""
        self.score = len(self.active_sensors) / len(self.sensors) if len(self.sensors) > 0 else 0

    def cleanup(self):
        i = 0
        tnow = time.time()
        while i < len(self.updates):
            with self.lock:
                ip, score, last = self.updates[i]

            #print(f"Checking update {i}: {self.updates[i]}. f{d > last}")

            # Remove old updates and subtract from score
            if tnow - TIMEOUT_STATE_UPDATE < last:
                break

            # print(f"Removing old update {i}: {self.updates[i]}")

            # the same node may have been active more recently
            # the active_indices value always reflects most recent
            with self.lock:
                if ip in self.active_sensors and self.active_sensors[ip] == last:
                    print(f"Marking {ip} inactive")
                    del self.active_sensors[ip]
                self.updates.pop(i)
            i += 1
        
        self.calc_score()

    def get_score(self):
        # scored in triggers per second over some sliding window?
        return len(self.active_sensors) # * 1000 / (time.time() - self.t_last_update) if self.t_last_update > 0 else 0

class Garden:
    def __init__(self):
        self.connections = {}
        self.last_response = {} # tracklast response time for each connection so we can timeout stale ones
        self.ip_to_mac = {}
        self.t_offsets = {} # map of time offsets for each connection
        self.last_score = -1 # used so we don't send the same score twice
        self.last_pat = 0
        self.lock = threading.Lock()

        self.wadsworth = {}
        self.wads = {}
        self.venus = {}
        self.squish = {}

        self.state = State()
        self.conf = Config(DEF_CONF)
        self.ping_timeout = 60 # Timeout in seconds for stale connections

        self.t_start_sun_detection = 0

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
                    print("ending message to", addr)
                    conn.sendall(message)
                    # print(f"Sent message to {addr}")
                except Exception as e:
                    print(f"Error sending message to {addr}: {e}")

    def loop(self):
        last_ping = 0
        last_state_update = time.time()

        force_state_update = False
        last_forced_state_update = time.time()

        while True:
            now = time.time()

            # Send periodic pings
            if now - last_ping > TIMEOUT_BROADCAST_PING:
                last_ping = now
                #print("Broadcasting ping to all clients...")
                self.broadcast(None, build_message(PROTO_PING, b"server ping"))
                self.log_state()

            # Clean up stale connections
            self.cleanup_stale_connections()

            # Force a state update every 10 seconds in case anyone new has joined
            if now - last_forced_state_update > TIMEOUT_FORCED_STATE_UPDATE_MESSAGE:
                force_state_update = True
                last_forced_state_update = now

            # Send state updates
            if now - last_state_update > 1:
                self.send_state_update(force_state_update)
                last_state_update = now
                force_state_update = False

            time.sleep(0.05)  # Sleep to avoid busy waiting

    def log_state(self):
        print(f"Active: {self.wads_active()}:")
        print(f"- Pattern: {self.state.pattern}, Score: {self.state.get_score():.2f}, Active: {len(self.state.active_sensors)}, Raw: {self.state.avg_raw_garden_score:.2f}")
        with self.lock:
            print("- Wadsworth:", list(self.wadsworth.keys()))
            print("- Wads:", self.wads.keys())
            print("- Venus:", self.venus.keys())
            print("- Squishies:", self.squish.keys())

    def handle_sensor(self, connection, index, pct, raw):
        # ignoring for now, just relying on pulses
        # TODO: all sensors should be batched together in a single message
        
        ip, _ = connection.getpeername()
        print(f"{ip}: sent us sensor data: {index}, {pct}, {raw}")

        # Update the state with the sensor data
        # Need the lock since it's periodically cleaned in the other thread
        #self.state.update_sensor(ip, index, pct, age)

        # keep track of average raw value for all wads
        self.state.update_sensor(ip, index, raw)

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
                print(f"Ident from: {ip}:{port}, millis: {millis}. It's a {name}. MAC: {mac}")
            
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

    def handle_sleepy_time_override(self, connection):
        """Handle a sleepy time override message."""
        ip, _ = connection.getpeername()
        print(f"{ip}: Received sleepy time override message")
        
        # Set the state to daytime
        self.state.set_daytime()

        # Broadcast the sleepy time message to all connections
        self.broadcast(None, build_message(PROTO_SLEEPY_TIME, b""))
        
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

    def send_state_update(self, force=False):
        if force and self.state.daytime:
            self.broadcast(None, build_message(PROTO_SLEEPY_TIME, b""))
            return

        pat = self.state.pattern
        act = self.wads_active()
        score = self.state.get_score() / act if act > 0 else 0
        score *= 100 # convert to percentage

        self.state.cleanup()

        if not force and score == self.last_score and self.last_pat == pat:
            #print(f"Skipping state update, score {score} and pat {pat} have no changed")
            return

        self.last_score = score

        payload = build_state_update(pat, int(score))

        print(f"Broadcasting state update message: Pattern: {pat} Score: {score} (Raw score {self.state.avg_raw_garden_score} out of {act})")
        self.broadcast(None, build_message(PROTO_STATE_UPDATE, payload))
