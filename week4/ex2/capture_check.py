#!/usr/bin/env python3
"""Bắt dữ liệu USART1 của week4/ex2 và kiểm tra.
Dùng:  python capture_check.py COM5 30        (bắt 30 s từ cổng COM5)
       python capture_check.py --file dump.bin (phân tích file đã lưu)
Cần: pip install pyserial  (chỉ khi bắt từ cổng COM)
"""
import struct, sys, time

FRAME = 16                 # sizeof(sensor_sample_t)
BLOCK = 64 * FRAME         # 1024 B
NOMINAL = 72_000_000 // 500  # 144000 chu kỳ DWT / 2 ms
#mở cổng COM ở 115200, đọc liên tục trong N giây, 
#gom mọi byte vào một biến, và trả về cùng thời gian đã bắt. main lưu nó ra dump.bin.
def capture(port, seconds):
    import serial
    ser = serial.Serial(port, 115200, timeout=0.1)
    ser.reset_input_buffer()
    data = bytearray()
    t0 = time.time()
    while time.time() - t0 < seconds:
        data += ser.read(4096)
    return bytes(data), time.time() - t0
# một mẫu gồm 16 byte:4byte timestamp đầu và 12 byte còn lại cho data
# bỏ off byte đầu, chia phần còn lại thành các khung 16 B,
#tính delta = timestamp sau trừ timestamp trước (phép & 0xFFFFFFFF để xử lý khi bộ đếm 32-bit tràn),
#đếm bao nhiêu delta gần 144.000 (lệch dưới 2000).
def analyze(data, elapsed):
    best = None
    for off in range(FRAME):                      # tìm điểm bắt đầu khung 16 B
        n = (len(data) - off) // FRAME
        ts = [f[0] for f in struct.iter_unpack('<I6h', data[off:off + n * FRAME])]
        d = [(b - a) & 0xFFFFFFFF for a, b in zip(ts, ts[1:])]
        ok = sum(1 for x in d if abs(x - NOMINAL) < 2000)
        if best is None or ok > best[0]:
            best = (ok, off, d)
    ok, off, d = best
    # nếu delta lớn hơn 1,5 × 144.000 thì có mẫu bị bỏ.
    dropped = sum(round(x / NOMINAL) - 1 for x in d if x > 1.5 * NOMINAL)
    odd = sum(1 for x in d if abs(x - NOMINAL) >= 2000 and x <= 1.5 * NOMINAL)
    print(f"tong byte        : {len(data)}")
    print(f"lech khung (off) : {off}")
    print(f"so mau           : {len(d) + 1}")
    print(f"delta = 2 ms     : {ok}/{len(d)}")
    print(f"mau bi mat       : {dropped}")
    print(f"delta bat thuong : {odd}")
    print(f"delta TB         : {sum(d)/len(d)/72:.1f} us (ly thuyet 2000)")
    if elapsed:
        print(f"byte/s           : {len(data)/elapsed:.0f} (ly thuyet 8000)")
        print(f"khoi/s           : {len(data)/BLOCK/elapsed:.4f} (ly thuyet 7.8125)")

if __name__ == "__main__":
    if sys.argv[1] == "--file":
        analyze(open(sys.argv[2], "rb").read(), None)
    else:
        data, el = capture(sys.argv[1], float(sys.argv[2]))
        open("dump.bin", "wb").write(data)
        analyze(data, el)