import datetime
import socket
import json
import time
import threading
import signal
import os
import sys

BOARDCAST_IP = "192.168.137.255"
stop_event = threading.Event()

fsa_v2_list = []
fsa_v3_list = []


def log_print(*args):
    timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
    msg = f"[{timestamp}] " + " ".join(str(a) for a in args)
    print(msg)


def print_summary(info: str):
    log_print(f"\n# =========================== {info} =========================== #")

    if fsa_v2_list:
        print("\n[FSA V2]")
        for fsa in fsa_v2_list:
            print(f"  - {fsa}")
    if fsa_v3_list:
        print("\n[FSA V3]")
        for fsa in fsa_v3_list:
            print(f"  - {fsa}")
    if not fsa_v2_list and not fsa_v3_list:
        print("当前没有任何 FSA 设备.")

    print("# ================================================================ #\n")


class FSA:
    def __init__(self, ip: str):
        self.ip = ip
        self.max_cnt = 20000000
        self.max_continuous_cnt = 2
        self.all_cnt = 0
        self.timeout_cnt = 0
        self.continuous_cnt = 0
        self.timeout_rate = 0.0

    def __repr__(self):
        return f"FSA(ip={self.ip}, 总包数={self.all_cnt}, 丢包数={self.timeout_cnt}, 丢包率=万分之{self.timeout_rate:.6f})"


def boardcast(data: str, port=2334, timeout=3.0):
    log_print(
        "# ------------------------------------ 广播 ------------------------------------ #"
    )

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.settimeout(timeout)
    addr = (BOARDCAST_IP, port)

    try:
        sock.sendto(data.encode("utf-8"), addr)
        start = time.time()
        while time.time() - start < timeout:
            if stop_event.is_set():
                break
            try:
                recv_data, from_addr = sock.recvfrom(1024)
                msg = recv_data.decode("utf-8")

                try:
                    json_data = json.loads(msg)
                except json.JSONDecodeError:
                    log_print(f"非JSON数据: {msg}")
                    continue

                ip = from_addr[0]
                ver = json_data.get("protocol_version", 2)

                if ver == 3:
                    if ip not in [f.ip for f in fsa_v3_list]:
                        fsa_v3_list.append(FSA(ip))
                else:
                    if ip not in [f.ip for f in fsa_v2_list]:
                        fsa_v2_list.append(FSA(ip))
            except socket.timeout:
                break
    finally:
        sock.close()
        print_summary("搜索结果")


def udp_timeout_test(fsa: FSA, sock, addr, msg):
    while not stop_event.is_set():
        sock.sendto(msg, addr)
        fsa.all_cnt += 1

        if fsa.all_cnt >= fsa.max_cnt:
            break

        try:
            recv_data, _ = sock.recvfrom(1024)
            fsa.continuous_cnt = 0
        except socket.timeout:
            fsa.timeout_cnt += 1
            fsa.continuous_cnt += 1
            print_summary("丢包")

        if fsa.continuous_cnt >= fsa.max_continuous_cnt:
            log_print(f"[警告] {fsa.ip} 连续丢包超过 {fsa.continuous_cnt} 个")

        fsa.timeout_rate = (fsa.timeout_cnt / fsa.all_cnt) * 10000

    sock.close()


def get_pvct_v2(fsa: FSA, timeout=0.2):
    log_print(f"# ---------- 启动 V2 测试: {fsa.ip} ---------- #")
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(timeout)
    addr = (fsa.ip, 2335)
    msg = bytes([0x1D])
    udp_timeout_test(fsa, sock, addr, msg)


def get_pvct_v3(fsa: FSA, timeout=0.2):
    log_print(f"# ---------- 启动 V3 测试: {fsa.ip} ---------- #")
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(timeout)
    addr = (fsa.ip, 2340)
    msg = bytes([0x00, 0x00, 0x00, 0x02, 0x34, 0x83, 0x64, 0x87])
    udp_timeout_test(fsa, sock, addr, msg)


def signal_handler(sig, frame):
    log_print("检测到 Ctrl+C, 立即停止所有线程!")
    stop_event.set()
    print_summary("测试结果")
    log_print("程序已强制退出.")
    os._exit(0)


if __name__ == "__main__":
    signal.signal(signal.SIGINT, signal_handler)

    msg = "Is any fourier smart server here?"
    boardcast(msg)

    log_print("测试开始, 按 Ctrl+C 可立即停止并打印当前状态.")

    threads = []

    for fsa in fsa_v2_list:
        t = threading.Thread(target=get_pvct_v2, args=(fsa,), daemon=True)
        t.start()
        threads.append(t)

    for fsa in fsa_v3_list:
        t = threading.Thread(target=get_pvct_v3, args=(fsa,), daemon=True)
        t.start()
        threads.append(t)

    try:
        while any(t.is_alive() for t in threads):
            time.sleep(1)
    except KeyboardInterrupt:
        signal_handler(None, None)
    finally:
        stop_event.set()
        print_summary("测试结果")
        log_print("测试完成, 程序正常退出.")
        sys.exit(0)
