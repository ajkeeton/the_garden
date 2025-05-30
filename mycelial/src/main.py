import socket
from handler import ProtocolHandler
from zeroconf import ServiceInfo, Zeroconf
from gardener import Garden
import threading

def handle_client(csock, addr, garden):
    #print(f"Connection from {addr}")
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
    gthread = threading.Thread(target=garden.loop)
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
