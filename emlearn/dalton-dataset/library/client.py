import socket
import json
import time
import random

# Server address and port
UDP_IP = "127.0.0.1"  # Use "localhost" or the server's IP if on a different machine
UDP_PORT = 12345

# Create UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

def generate_sensor_data():
    """Generate random sensor data similar to Arduino output."""
    data = {
        "T": round(random.uniform(20.0, 30.0), 1),
        "H": round(random.uniform(40.0, 60.0), 1),
        "FMHDS": random.randint(0, 1),
        "PMS1": random.randint(0, 100),
        "PMS2_5": random.randint(0, 100),
        "PMS10": random.randint(0, 100),
        "NO2": random.randint(150, 200),
        "C2H5OH": random.randint(200, 400),
        "VoC": random.randint(700, 900),
        "CO": random.randint(50, 100),
        "CO2": random.randint(400, 600)
    }
    return json.dumps(data)

try:
    while True:
        # Generate and send data
        message = generate_sensor_data()
        sock.sendto(message.encode('utf-8'), (UDP_IP, UDP_PORT))
        print("Sent message:", message)
        
        # Wait before sending the next message
        time.sleep(2)  # Adjust as needed
except KeyboardInterrupt:
    print("Client stopped.")
finally:
    sock.close()
