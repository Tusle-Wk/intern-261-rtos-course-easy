#script mở cổng COM4 và gửi liên tiếp 500 lệnh dạng P10#0\r, P10#1\r, ..., P10#499\r
#Trong lúc gửi, một luồng đọc riêng thu lại toàn bộ log mà board in ra. Sau đó script 
#đếm xem board đã xử lý đúng bao nhiêu lệnh trong 500 lệnh đã gửi, mất bao nhiêu và lệnh nào bị mất.

import re, sys, time, threading, serial

port, n, gap_ms = sys.argv[1], int(sys.argv[2]), float(sys.argv[3])
ser = serial.Serial(port, 115200, timeout=0.05)
ser.reset_input_buffer()

rx, stop = bytearray(), threading.Event()
def reader():
    while not stop.is_set():
        rx.extend(ser.read(4096))
threading.Thread(target=reader, daemon=True).start()

cmds = [f"P10#{k}\r".encode() for k in range(n)]
total = sum(map(len, cmds))

t0 = time.perf_counter()
if gap_ms == 0:
    ser.write(b"".join(cmds))
else:
    for k, c in enumerate(cmds):
        while time.perf_counter() < t0 + k * gap_ms / 1000:
            pass
        ser.write(c)
ser.flush()
t1 = time.perf_counter()

time.sleep(1.5)
stop.set()

seqs = [int(m) for m in re.findall(rb"-> 10 ms #(\d+)", bytes(rx))]
good_list, last = [], -1
for s in seqs:
    if s > last:
        good_list.append(s)
        last = s
good = len(good_list)
good_set = set(good_list)
lost_list = [k for k in range(n) if k not in good_set]
print(f"gui {n} lenh ({total} B) trong {t1 - t0:.3f} s -> {total / (t1 - t0):.0f} B/s (phia PC)")
print(f"nhan dung {good}, mat/loi {n - good}, lenh mat: {lost_list[:10]}")
ser.close()