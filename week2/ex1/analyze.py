"""Phan tich 1 lan do cua week2/ex1.

Cach dung (chay trong thu muc chua cac file .bin / .txt):
    python analyze.py A            -> doc log_raw_A.bin, log_filt_A.bin, term_A.txt
    python analyze.py B

Dau ra: in thong ke ra man hinh va luu queue_fill_<lan>.png
"""
import os
import re
import sys

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
#doc tag sau ten file de tim file co format tuong tu de chạy
tag = sys.argv[1] if len(sys.argv) > 1 else "A"
f_raw, f_filt, f_term = f"log_raw_{tag}.bin", f"log_filt_{tag}.bin", f"term_{tag}.txt"

# Do day cua queue 
if os.path.exists(f_raw) and os.path.exists(f_filt):
    raw = np.fromfile(f_raw, dtype=np.uint8)
    filt = np.fromfile(f_filt, dtype=np.uint8)
    t = np.arange(len(raw))  # 1 phan tu = 1 ms (LOG_PERIOD_MS)

    print(f"=== Lan {tag}: do day queue ({len(raw)} mau, 1 ms/mau) ===")
    print(f"queue_raw     : max = {raw.max()} | so diem > 0 = {int((raw > 0).sum())}")
    print(f"queue_filtered: max = {filt.max()} | so diem > 0 = {int((filt > 0).sum())}")

    nz = np.flatnonzero(filt > 0)
    if len(nz) > 1:
        vals, cnt = np.unique(np.diff(nz), return_counts=True)
        top = sorted(zip(cnt, vals), reverse=True)[:5]
        print("khoang cach giua cac diem filt > 0 (ms, so lan):",
              [(int(v), int(c)) for c, v in top])
    if len(nz) > 0:
        start = int(nz[0])
        print(f"queue_filtered bat dau > 0 tai {start} ms")
        full = np.flatnonzero(filt >= 6)
        if len(full) > 0:
            print(f"queue_filtered day (>=6) lan dau tai {int(full[0])} ms "
                  f"(sau {int(full[0]) - start} ms ke tu luc bat dau tang)")
        else:
            print("queue_filtered KHONG bao gio day (>=6)")

    plt.figure(figsize=(10, 4))
    plt.step(t, raw, where="post", label="queue_raw")
    plt.step(t, filt, where="post", label="queue_filtered")
    plt.xlabel("t (ms)")
    plt.ylabel("do day (so phan tu)")
    plt.ylim(0, 6.5)
    plt.title(f"Do day queue theo thoi gian - lan {tag}")
    plt.legend()
    plt.tight_layout()
    out = f"queue_fill_{tag}.png"
    plt.savefig(out, dpi=150)
    print(f"Da luu {out}")
else:
    print(f"[bo qua] khong thay {f_raw} hoac {f_filt}")

# ---------- 2. Dem 'lo' trong seq cua log terminal ----------
if os.path.exists(f_term):
    pat = re.compile(r"^(\d+),-?\d+,-?\d+,-?\d+\s*$")
    seqs = []
    with open(f_term, errors="ignore") as fh:
        for line in fh:
            m = pat.match(line)
            if m:
                seqs.append(int(m.group(1)))
    print(f"\n=== Lan {tag}: seq trong {f_term} ===")
    if not seqs:
        print("Khong doc duoc dong hop le nao. Dinh dang file co khac 'seq,ax,ay,az' khong?")
    else:
        lost, first_lost = 0, None
        for a, b in zip(seqs, seqs[1:]):
            if b > a + 1:
                lost += b - a - 1
                if first_lost is None:
                    first_lost = a + 1
        print(f"so dong hop le = {len(seqs)} | seq dau = {seqs[0]} | seq cuoi = {seqs[-1]}")
        print(f"so mau mat (lo trong seq) = {lost} | mau mat dau tien = {first_lost}")
        print("30 seq dau:", seqs[:30])
else:
    print(f"[bo qua] khong thay {f_term}")
if os.path.exists("send_tick_T4.bin"):
    t = np.fromfile("send_tick_T4.bin", dtype=np.uint32)[:200]
    print("send_tick diff (ms):", np.diff(t)[:40])