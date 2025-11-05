#!/usr/bin/python3

import datetime
import socket
import json
import time
import threading

BOARDCAST_IP = "192.168.137.255"


class FSA:
    def __init__(self, ip: str):
        self.ip = ip
        self.max_cnt = 100000
        self.max_continuous_cnt = 2
        self.all_cnt = 0
        self.timeout_cnt = 0
        self.continuous_cnt = 0
        self.timeout_rate = 0.0

    def __repr__(self):
        return f"FSA(ip={self.ip}, 总包数={self.all_cnt}, 丢包数={self.timeout_cnt}, 丢包率={self.timeout_rate:.6f}%)"


fsa_v2_list = []
fsa_v3_list = []


def log_print(*args):
    timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
    msg = f"[{timestamp}] " + " ".join(str(a) for a in args)
    print(msg)


def boardcast(data: str, port=2334, timeout=1.0):
    log_print(
        "# ------------------------------------ 广播 ------------------------------------ #"
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

        if fsa_v2_list:
            log_print(f"v2: {fsa_v2_list}")

        if fsa_v3_list:
            log_print(f"v3: {fsa_v3_list}")

        if not fsa_v2_list and not fsa_v3_list:
            log_print(f"未搜索到执行器, 请检查是否连接!")


def udp_timeout_test(fsa: FSA, sock, addr, msg):
    while True:
        sock.sendto(msg, addr)
        fsa.all_cnt += 1

        if fsa.all_cnt >= fsa.max_cnt:
            return

        try:
            recv_data, from_addr = sock.recvfrom(1024)
            fsa.continuous_cnt = 0
        except socket.timeout:
            fsa.timeout_cnt += 1
            fsa.continuous_cnt += 1

        if fsa.continuous_cnt >= fsa.max_continuous_cnt:
            log_print(f"[警告] {fsa.ip} 连续丢包超过 {fsa.continuous_cnt} 次")

        fsa.loss_rate = (fsa.timeout_cnt / fsa.all_cnt) * 100


def get_pvct_v2(fsa: FSA, timeout=0.2):
    log_print(
        "# ---------------------------------- V2 测试 ---------------------------------- #"
    )

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(timeout)
    addr = (fsa.ip, 2335)
    msg = bytes([0x1D])

    udp_timeout_test(fsa, sock, addr, msg)
    log_print(f"v2: {fsa_v2_list}")


def get_pvct_v3(fsa: FSA, timeout=0.2):
    log_print(
        "# ---------------------------------- V3 测试 ---------------------------------- #"
    )

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(timeout)
    addr = (fsa.ip, 2340)
    msg = bytes([0x0, 0x0, 0x00, 0x2, 0x34, 0x83, 0x64, 0x87])

    udp_timeout_test(fsa, sock, addr, msg)
    log_print(f"v3: {fsa_v3_list}")


if __name__ == "__main__":
    msg = "Is any fourier smart server here?"
    boardcast(msg)

    threads = []

    for fsa in fsa_v2_list:
        t = threading.Thread(target=get_pvct_v2, args=(fsa,), daemon=True)
        t.start()
        threads.append(t)

    for fsa in fsa_v3_list:
        t = threading.Thread(target=get_pvct_v3, args=(fsa,), daemon=True)
        t.start()
        threads.append(t)

    for t in threads:
        t.join()
