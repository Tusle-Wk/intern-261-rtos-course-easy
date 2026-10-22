#!/usr/bin/env python3
"""Thong ke jitter / drift cho week1/ex3 tu CSV xuat boi PulseView.

Chay 4 lan, moi cau hinh 1 file CSV:
  python3 analyze_ex3.py cfg1.csv --tick-hz 1000 --label "vTaskDelay 1000"
  python3 analyze_ex3.py cfg2.csv --tick-hz 1000 --label "xTaskDelayUntil 1000"
  python3 analyze_ex3.py cfg3.csv --tick-hz 100  --label "vTaskDelay 100"
  python3 analyze_ex3.py cfg4.csv --tick-hz 100  --label "xTaskDelayUntil 100"

Chan GPIO toggle 1 lan moi chu ky => khoang cach giua 2 canh LIEN TIEP
(len hoac xuong) chinh la 1 chu ky cua task.
"""
import argparse # xử lý các tham số truyền từ dòng lệnh
import math
import re # biểu thức chính quy (Regex) để bóc tách chuỗi đơn vị, tần số
import statistics # tính độ lệch chuẩn
import sys # tương tác hệ thống (thoát chương trình khi gặp lỗi sys.exit)

PERIOD_MS_DEFAULT = 17.0   # chu kỳ mục tiêu (17 ms)   
SAMPLE_COUNT_DEFAULT = 800    # số chu kỳ đo đạc tối thiểu yêu cầu (800 chu kỳ)
# bảng tra cứu để quy đổi các đơn vị thời gian thường gặp trong file CSV về giây
UNIT_SCALE = {"s": 1.0, "ms": 1e-3, "us": 1e-6, "µs": 1e-6, "ns": 1e-9}

# PulseView (Label values = units) đặt tên cột với thời gian là đơn vị chính
UNIT_NAMES = {"seconds": 1.0, "milliseconds": 1e-3, "microseconds": 1e-6,
              "nanoseconds": 1e-9}


def parse_samplerate(text):
    # tìm kiếm cụm số kèm tiền tố đơn vị k, m, g
    m = re.search(r"([\d.]+)\s*([kKmMgG]?)", text)
    if not m:
        return None
    # bảng nhân tỷ lệ: k (kilo = 10^3), m (mega = 10^6), g (giga = 10^9)
    mult = {"": 1.0, "k": 1e3, "m": 1e6, "g": 1e9}[m.group(2).lower()]
    return float(m.group(1)) * mult


def pick_columns(state_cols, labels, channel):
    # xác định đúng cột dữ liệu của kênh GPIO cần phân tích từ danh sách các cột.
    if channel is None:
        return state_cols
    for c in state_cols:
        if labels[c].lower() == channel.lower():
            return [c]
    if channel.isdigit() and int(channel) < len(state_cols):
        return [state_cols[int(channel)]]
    sys.exit(f"Khong tim thay kenh '{channel}'. Cac kenh co: "
             + ", ".join(labels[c] for c in state_cols))


def load_edges(path, channel, samplerate):
    """Doc CSV, tra ve ({ten_kenh: [thoi_diem_canh_giay]}, samplerate hoac None).

    Ho tro 2 dang:
      - co cot thoi gian (header chua 'time'): dung truc tiep.
      - moi dong 1 mau, khong co cot thoi gian (dang libsigrok CSV): thoi gian =
        chi so dong / samplerate (lay tu dong '; Samplerate: ...' hoac --samplerate).
    """
    # đọc file CSV theo từng dòng =để tránh tràn RAM khi file nặng hàng chục MB.
    # phát hiện các thời điểm chuyển mức logic (0->1 hoặc 1->0) và lưu vào danh sách mốc thời gian.
    names = None
    time_col = None
    time_scale = 1.0
    file_rate = None
    rate = samplerate
    tracked = None
    labels, prev, edges = {}, {}, {}
    first = True
    idx = 0

    with open(path, newline="", encoding="utf-8-sig") as f:
        for raw in f:
            line = raw.strip()
            if not line:
                continue
            # đọc dòng ghi chú của PulseView chứa tần số mẫu (ví dụ: samplerate: 1 MHz)
            if line.startswith(";"):
                m = re.match(r";\s*samplerate\s*:\s*(.+)", line, re.I)
                if m:
                    file_rate = parse_samplerate(m.group(1))
                continue

            cells = [c.strip() for c in line.split(",")]
            # xử lý dòng dữ liệu hợp lệ đầu tiên để kiểm tra có header chữ hay không
            if first:
                first = False
                try:
                    [float(c) for c in cells]
                except ValueError:                      # dong tieu de
                    # nếu parse được sang số ngay -> file không có dòng tiêu đề chữ
                    names = cells
                    for i, n in enumerate(names):
                        low = n.lower()
                        if low in UNIT_NAMES:
                            time_col, time_scale = i, UNIT_NAMES[low]
                        elif "time" in low:
                            time_col = i
                            m = re.search(r"[\[(]\s*(ns|us|µs|ms|s)\s*[\])]", low)
                            if m:
                                time_scale = UNIT_SCALE[m.group(1)]
                    continue
            # chuyển đổi toàn bộ giá trị trong dòng hiện tại sang kiểu số thực
            try:
                vals = [float(c) for c in cells]
            except ValueError:
                continue
            # khởi tạo thông tin các kênh logic ở dòng mẫu dữ liệu đầu tiên
            if tracked is None:
                if rate is None:
                    rate = file_rate
                if time_col is None and rate is None:
                    sys.exit("CSV khong co cot thoi gian va khong co dong '; Samplerate'. "
                             "Hay truyen --samplerate <Hz>.")
                # các cột còn lại (không phải cột thời gian) chính là các kênh tín hiệu logic
                state_cols = [i for i in range(len(vals)) if i != time_col]
                # gán nhãn tên kênh, ưu tiên tên trong file, nếu trùng thì đặt là D0, D1,...
                have_names = (bool(names) and all(c < len(names) for c in state_cols)
                              and len({names[c] for c in state_cols}) == len(state_cols))
                for k, c in enumerate(state_cols):
                    labels[c] = names[c] if have_names else f"D{k}"
                tracked = pick_columns(state_cols, labels, channel)
                for c in tracked:
                    edges[c], prev[c] = [], None    # khởi tạo mảng lưu cạnh xung cho kênh

            # xác định mốc thời gian t (giây) của mẫu hiện tại:
            # - có cột time: lấy giá trị nhân tỷ lệ
            # - không có cột time: lấy (thứ tự mẫu / samplerate)
            t = vals[time_col] * time_scale if time_col is not None else idx / rate
            idx += 1
            #edge detection
            for c in tracked:
                s = 1 if vals[c] >= 0.5 else 0
                if prev[c] is not None and s != prev[c]:
                    edges[c].append(t)
                prev[c] = s

    if tracked is None:
        sys.exit("CSV khong co dong du lieu nao.")
    return {labels[c]: edges[c] for c in tracked}, (rate or file_rate)


def main():
    # cấu hình nhận tham số CLI
    ap = argparse.ArgumentParser(description="Thong ke jitter/drift week1/ex3")
    ap.add_argument("csv")
    ap.add_argument("--tick-hz", type=int, required=True,
                    help="configTICK_RATE_HZ cua lan chay nay (1000 hoac 100)")
    ap.add_argument("--period-ms", type=float, default=PERIOD_MS_DEFAULT)
    ap.add_argument("--samples", type=int, default=SAMPLE_COUNT_DEFAULT)
    ap.add_argument("--channel", default=None,
                    help="ten kenh (vd D1) hoac chi so; mac dinh: tu chon kenh co canh")
    ap.add_argument("--samplerate", type=float, default=None,
                    help="Hz; chi can khi CSV khong co cot thoi gian / dong Samplerate")
    ap.add_argument("--label", default="")
    args = ap.parse_args()

    # Giống pdMS_TO_TICKS: (ms * hz) / 1000
    # mô phỏng phép tính chia số nguyên của macro FreeRTOS: pdMS_TO_TICKS(period)
    # Ví dụ với 17ms:
    # Ở 1000Hz: (17 * 1000) // 1000 = 17 ticks -> eff_ms = 17 * 1000 / 1000 = 17.0 ms
    # Ở 100Hz:  (17 * 100) // 1000  = 1 tick   -> eff_ms = 1 * 1000 / 100 = 10.0 ms
    ticks = int(args.period_ms * args.tick_hz // 1000)
    if ticks < 1:
        sys.exit("PERIOD_MS < 1 tick o tick rate nay.")
    eff_ms = ticks * 1000.0 / args.tick_hz   # chu ky ma kernel thuc su duoc yeu cau

    print(f"Dang phan tich: {args.csv}")
    by_ch, rate = load_edges(args.csv, args.channel, args.samplerate)

    # lọc kênh có cạnh xung tín hiệu hoạt động
    active = {k: v for k, v in by_ch.items() if len(v) >= 2}
    if not active:
        sys.exit("Khong kenh nao co canh. Kiem tra day / kenh / thoi gian bat.")
    if len(active) > 1:
        print("CANH BAO: nhieu kenh co canh:", ", ".join(f"{k}({len(v)})" for k, v in active.items()),
              "-> chon kenh nhieu canh nhat. Dung --channel de chi ro.")
    #tự động chọn kênh có nhiều cạnh xung nhất để phân tích
    ch = max(active, key=lambda k: len(active[k]))
    t = active[ch]
    # kiểm tra số lượng chu kỳ đã thu thập
    n = args.samples
    if len(t) - 1 < n:
        print(f"CANH BAO: chi co {len(t) - 1} chu ky, yeu cau {n} -> KHONG DAT SAMPLE_COUNT.")
        n = len(t) - 1
    if n < 1:
        sys.exit("Khong du canh de tinh chu ky.")

    #tính toán các đại lượng chu kỳ
    periods = [(t[i] - t[i - 1]) * 1e3 for i in range(1, n + 1)]   # ms
    mn, mx = min(periods), max(periods)
    avg = sum(periods) / n
    sd = statistics.pstdev(periods)
    jitter_us = (mx - mn) * 1e3 #tính jitter

    total_ms = (t[n] - t[0]) * 1e3  #tính drift
    drift_eff = total_ms - n * eff_ms          # so với chu kỳ kernel được yêu cầu
    drift_req = total_ms - n * args.period_ms  # so với PERIOD_MS thiết kế

    #in ra màn hình
    print("-" * 56)
    print(f"Kenh                  : {ch}")
    print(f"So chu ky             : {n}")
    if rate:
        print(f"Do phan giai thoi gian: {1e6 / rate:.2f} us (jitter nho hon muc nay khong co y nghia)")
    print(f"pdMS_TO_TICKS({args.period_ms:g})   : {ticks} tick = {eff_ms:.3f} ms @ {args.tick_hz} Hz")
    print(f"Min                   : {mn:.4f} ms")
    print(f"Max                   : {mx:.4f} ms")
    print(f"Trung binh            : {avg:.4f} ms")
    print(f"Do lech chuan         : {sd * 1e3:.2f} us")
    print(f"Jitter (Max - Min)    : {jitter_us:.2f} us")
    print(f"Tong thoi gian        : {total_ms:.3f} ms")
    print(f"Drift vs {eff_ms:g} ms (kernel) : {drift_eff:+.3f} ms   <- dung cho 'xTaskDelayUntil ~ 0'")
    print(f"Drift vs {args.period_ms:g} ms (PERIOD_MS): {drift_req:+.3f} ms   <- dung cho phan sai so lam tron tick")
    print("-" * 56)
    print("Dong cho REPORT (min | max | tb | jitter us | drift kernel ms | drift PERIOD_MS ms):")
    print(f"| {args.label} | {mn:.4f} | {mx:.4f} | {avg:.4f} | {jitter_us:.1f} | "
          f"{drift_eff:+.2f} | {drift_req:+.2f} |")


if __name__ == "__main__":
    main()