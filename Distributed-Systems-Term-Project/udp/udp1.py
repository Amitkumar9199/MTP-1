import asyncio
import json
import signal
import socket
from pprint import pp
import random
from typing import Any, Counter, Dict, Tuple


time_cnt = 0
info: Dict[str, Tuple[str, int]] = {
    "esp1": ("unknown", -1),
    "cpu1": ("unknown", -1),
    "cpu2": ("unknown", -1),
}


def signal_handler(signal, frame):
    print('You pressed Ctrl+C!')
    if s:
        s.close()
    exit(0)


signal.signal(signal.SIGINT, signal_handler)

# Listen on Port
PORT = 10000
BROADCAST = "192.168.137.255"
# Size of receive buffer
BUFFER_SIZE = 1024

# Create a UDP socket
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
# Bind the socket to the host and port
s.bind(("", PORT))
print(f"UDP Server is listening on ip {s.getsockname()[0]} "
      f"and port {s.getsockname()[1]}")

if not s:
    print("Failed to create socket")
    exit(1)


def udp_server():
    while True:
        # Receive BUFFER_SIZE bytes data
        # data is a list with 2 elements
        # first is data
        # second is client address
        data = s.recvfrom(BUFFER_SIZE)
        if data:
            # decode message
            msg: Dict[str, Any] = json.loads(data[0].decode('utf-8'))
            # data = json.dumps(msg, indent=4)

            msg_id = msg.get('id', 'unknown')

            # If loopback, ignore
            if msg_id == "cpu1":
                continue

            # print received data
            print(f'Received msg {msg}')

            with open("comm_logs_1.txt", "a") as f:
                f.write(f"[ {data[1][0]}:{data[1][1]} ]: {msg}\n")

            if (msg_id == "admin"):
                cmd = msg.get('cmd', "")

                if cmd == "reset":
                    with open("comm_logs_1.txt", "w") as f:
                        pass

                    with open("info_1.txt", "w") as f:
                        pass

                    for key in info.keys():
                        info[key] = ("unknown", -1)
                    continue

            msg_state = msg.get('state')
            msg_time = msg.get('time', -1)

            if (msg_id in info.keys() and
                    info[msg_id][1] < msg_time and
                    msg_state is not None):
                info[msg_id] = (msg_state, msg_time)

    # Close connection
    s.close()
# END udp_server


def udp_client(msg: Dict[str, Any]):
    # broadcast message
    s.sendto(json.dumps(msg).encode('utf-8'), (BROADCAST, PORT))
    s.sendto(json.dumps(msg).encode('utf-8'), (BROADCAST, PORT+1))

# END udp_clinet


async def task():
    global time_cnt
    state = ["fire", "not fire"]

    while True:
        msg = {
            "id": "cpu1",
            "state": random.choice(state),
            "time": info["cpu1"][1] + 1,
        }
        info["cpu1"] = (msg["state"], msg["time"])
        udp_client(msg)

        await asyncio.sleep(5)
    # END while True
# END task


async def print_info():
    while True:
        print("Info: ")
        pp(info)

        cnt = Counter([v[0] for v in info.values()])
        print(f"Prediction: {cnt.most_common(1)}")

        with open("info_1.txt", "w") as f:
            json.dump(info, f, indent=4)

        await asyncio.sleep(2)
# END print_info


async def main():
    global info

    try:
        with open("info_1.txt", "r") as f:
            data = f.read()
            if data:
                info = json.loads(data)
    except Exception:
        pass

    await asyncio.gather(
        asyncio.to_thread(udp_server),
        task(),
        print_info(),
    )

# END main


if __name__ == "__main__":
    asyncio.run(main())
    # main()
