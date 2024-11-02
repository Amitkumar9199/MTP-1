import json
import socket
from typing import Any, Dict

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

PORT = 10000
BROADCAST = "10.5.20.255"


def udp_client(msg: Dict[str, Any]):
    # broadcast message
    s.sendto(json.dumps(msg).encode('utf-8'), (BROADCAST, PORT))
    # s.sendto(json.dumps(msg).encode('utf-8'), (BROADCAST, PORT+1))


udp_client({
    "id": "admin",
    "cmd": "reset",
})
