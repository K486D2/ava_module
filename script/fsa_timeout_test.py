import socket
import json
import time

BOARDCAST_IP = "192.168.137.255"


class FSA:
    def __init__(self, ip: str):
        self.ip = ip
        self.max_cnt = 100000
        self.all_cnt = 0
        self.timeout_cnt = 0
        self.timeout_rate = 0.0

    def __repr__(self):
        return f"FSA(ip={self.ip}, 总包数={self.all_cnt}, 丢包数={self.timeout_cnt}, 丢包率={self.timeout_rate:.6f}%)"


fsa_v2_list = []
fsa_v3_list = []


def boardcast(data: str, port=2334, timeout=1.0):
    print(
        "\n# ------------------------------------ 广播 ------------------------------------ #"
    )

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.settimeout(timeout)
    addr = (BOARDCAST_IP, port)

    try:
        sock.sendto(data.encode("utf-8"), addr)

        # 等待回复直到超时
        start = time.time()
        while time.time() - start < timeout:
            try:
                recv_data, from_addr = sock.recvfrom(1024)
                msg = recv_data.decode("utf-8")

                try:
                    json_data = json.loads(msg)
                except json.JSONDecodeError:
                    print(f"非JSON数据: {msg}")
                    continue

                ip = from_addr[0]
                ver = json_data.get("protocol_version", 2)

                if ver == 3:
                    if ip not in fsa_v3_list:
                        fsa_v3_list.append(FSA(ip))
                else:
                    if ip not in fsa_v2_list:
                        fsa_v2_list.append(FSA(ip))

            except socket.timeout:
                break

    finally:
        sock.close()
        print(f"v2: {fsa_v2_list}")
        print(f"v3: {fsa_v3_list}")


def get_pvct_v2(fsa: FSA, timeout=0.2):
    print(
        "\n# ---------------------------------- V2 测试 ---------------------------------- #"
    )

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(timeout)
    addr = (fsa.ip, 2335)

    while True:
        sock.sendto(bytes([0x1D]), addr)
        fsa.all_cnt += 1

        if fsa.all_cnt >= fsa.max_cnt:
            print(f"v2: {fsa_v2_list}")
            return

        try:
            recv_data, from_addr = sock.recvfrom(1024)
        except socket.timeout:
            fsa.timeout_cnt += 1

        fsa.loss_rate = (fsa.timeout_cnt / fsa.all_cnt) * 100


def get_pvct_v3(fsa: FSA, timeout=0.2):
    print(
        "\n# ---------------------------------- V3 测试 ---------------------------------- #"
    )

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(timeout)
    addr = (fsa.ip, 2340)

    while True:
        sock.sendto(bytes([0x0, 0x0, 0x00, 0x2, 0x34, 0x83, 0x64, 0x87]), addr)
        fsa.all_cnt += 1

        if fsa.all_cnt >= fsa.max_cnt:
            print(f"v3: {fsa_v3_list}")
            return

        try:
            recv_data, from_addr = sock.recvfrom(1024)
        except socket.timeout:
            fsa.timeout_cnt += 1

        fsa.loss_rate = (fsa.timeout_cnt / fsa.all_cnt) * 100


if __name__ == "__main__":
    msg = "Is any fourier smart server here?"
    boardcast(msg)

    for fsa in fsa_v2_list:
        get_pvct_v2(fsa)

    for fsa in fsa_v3_list:
        get_pvct_v3(fsa)
