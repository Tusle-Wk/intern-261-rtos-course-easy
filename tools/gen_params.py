#!/usr/bin/env python3
"""Sinh tham so rieng cho tung sinh vien.

    python3 tools/gen_params.py <student_id>

Tao `weekN/exM/app/params.h` va `weekN/exM/PARAMS.md` cho moi bai da ton tai.
Cung mot student_id luon cho ra cung mot bo so (deterministic).
Moi bo so da duoc chan de luon chay duoc tren 20 KB RAM cua STM32F103C8T6.
"""
import hashlib
import random
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def rng_for(student_id: str, key: str) -> random.Random:
    """RNG doc lap cho tung bai -> doi mot bai khong lam doi bai khac."""
    seed = hashlib.sha256(f"{student_id}|{key}".encode()).digest()
    return random.Random(int.from_bytes(seed[:8], "big"))


# --- Tham so tung bai ------------------------------------------------------
# Moi ham tra ve (params, notes): params = {TEN: (gia_tri, don_vi, giai_thich)}

def w1ex1(r):
    return {
        "BLINK_PERIOD_MS": (r.choice([83, 97, 113, 127, 149]), "ms",
                            "Chu ky nhay LED cua task bring-up"),
        "BLINK_GPIO_PIN": (r.choice([0, 1, 12, 13]), "PA<n>",
                           "Chan GPIO dung cho LED/logic analyzer"),
        "BRINGUP_STACK_WORDS": (r.choice([96, 112, 128]), "words",
                                "Stack cap cho task bring-up"),
    }


def w1ex2(r):
    # 3 task GPIO, uu tien doi mot -> starvation phai quan sat duoc
    prios = r.sample([1, 2, 3, 4], 3)
    work = r.sample([120, 180, 240, 300, 360, 420], 3)
    return {
        "TASK_A_PRIO": (prios[0], "", "Uu tien task A"),
        "TASK_B_PRIO": (prios[1], "", "Uu tien task B"),
        "TASK_C_PRIO": (prios[2], "", "Uu tien task C"),
        "TASK_A_WORK_US": (work[0], "us", "Thoi gian busy-loop moi vong cua A"),
        "TASK_B_WORK_US": (work[1], "us", "Thoi gian busy-loop moi vong cua B"),
        "TASK_C_WORK_US": (work[2], "us", "Thoi gian busy-loop moi vong cua C"),
        "TASK_A_PIN": (r.choice([0, 1]), "PA<n>", "Chan GPIO task A"),
        "TASK_B_PIN": (r.choice([4, 5]), "PA<n>", "Chan GPIO task B"),
        "TASK_C_PIN": (r.choice([6, 7]), "PA<n>", "Chan GPIO task C"),
    }


def w1ex3(r):
    # Chu ky co tinh KHONG chia het cho 10 ms -> lo ro sai so lam tron tick 100 Hz
    period = r.choice([7, 11, 13, 17, 19, 23])
    return {
        "PERIOD_MS": (period, "ms", "Chu ky can giu cua task tuan hoan"),
        # Phai > 1 tick (1000 us o cau hinh 1 kHz), khong chi "gan bang", de vong
        # lap busy-wait vuot ranh gioi tick truoc khi goi vTaskDelay(). Neu khong,
        # vTaskDelay va xTaskDelayUntil cho ket qua giong het nhau o 1 kHz (khong
        # the tao drift), lam rubric "drift cua vTaskDelay KHONG xap xi 0" khong
        # the dat duoc bang do that. Xem bugs.md #4, mutations.md M1.
        "WORK_JITTER_US": (r.choice([1100, 1400, 1800, 2400]), "us",
                           "Tai gia bien doi trong than vong lap"),
        "SAMPLE_COUNT": (r.choice([500, 800, 1000]), "lan",
                         "So chu ky phai do de tinh jitter"),
        "PERIODIC_PIN": (r.choice([0, 1, 6, 7]), "PA<n>",
                         "Chan GPIO toggle moi chu ky (do bang logic analyzer)"),
    }


def w1ex4(r):
    return {
        "HEAP_SIZE_BYTES": (r.choice([8192, 9216, 10240, 11264]), "bytes",
                            "configTOTAL_HEAP_SIZE phai dat dung gia tri nay"),
        "DYN_TASK_STACK_WORDS": (r.choice([96, 128, 160]), "words",
                                 "Stack cua task tao bang xTaskCreate"),
        "STATIC_TASK_STACK_WORDS": (r.choice([96, 128, 160]), "words",
                                    "Stack cua task tao bang xTaskCreateStatic"),
        "HOG_DEPTH_BYTES": (r.choice([256, 384, 512, 640]), "bytes",
                            "Kich thuoc mang tren stack dung de day task tran stack"),
        "TARGET_FREE_HEAP_BYTES": (r.choice([2048, 3072, 4096]), "bytes",
                                   "Heap con trong toi thieu phai chung minh sau khi khoi dong"),
    }


def w2ex1(r):
    return {
        "SAMPLE_PERIOD_MS": (r.choice([4, 5, 8, 10]), "ms", "Chu ky doc MPU6050"),
        "QUEUE_LENGTH": (r.choice([4, 6, 8, 12]), "phan tu", "Do dai queue giua cac tang"),
        "FILTER_WINDOW": (r.choice([4, 8, 16]), "mau", "Cua so trung binh truot"),
        "UART_BAUD": (r.choice([57600, 115200, 230400]), "baud", "Toc do USART1"),
    }


def w2ex2(r):
    return {
        "WRITER_COUNT": (r.choice([3, 4]), "task", "So task cung ghi UART"),
        "MSG_LEN": (r.choice([24, 32, 40]), "bytes", "Do dai moi ban tin"),
        "WRITER_PERIOD_MS": (r.choice([3, 5, 7]), "ms", "Chu ky phat cua moi writer"),
        "GATEKEEPER_PRIO": (r.choice([2, 3]), "", "Uu tien task gatekeeper"),
    }


def w2ex3(r):
    return {
        "LOW_HOLD_MS": (r.choice([8, 12, 16, 20]), "ms",
                        "Thoi gian task uu tien thap giu tai nguyen"),
        "MID_WORK_MS": (r.choice([20, 30, 40]), "ms",
                        "Thoi gian chiem CPU cua task uu tien trung"),
        "HIGH_PERIOD_MS": (r.choice([25, 35, 50]), "ms",
                           "Chu ky cua task uu tien cao"),
        "DEADLINE_MS": (r.choice([15, 20, 25]), "ms",
                        "Deadline cua task uu tien cao — phai chung minh dat/khong dat"),
    }


def w2ex4(r):
    return {
        "EVENT_BIT_COUNT": (r.choice([3, 4]), "bit", "So nguon su kien phai cho"),
        "NOTIFY_PERIOD_MS": (r.choice([2, 3, 5]), "ms", "Chu ky phat notification"),
        "SYNC_TIMEOUT_MS": (r.choice([50, 80, 120]), "ms", "Timeout khi cho dong bo"),
    }


def w3ex1(r):
    return {
        "EXTI_PIN": (r.choice([0, 1, 10, 11]), "PB<n>", "Chan nut dung cho EXTI"),
        "ACK_PIN": (r.choice([6, 7, 8]), "PA<n>",
                    "Chan task toggle de do latency ISR -> task"),
        "ISR_NVIC_PRIO": (r.choice([6, 7, 8]), "", "Uu tien NVIC cho EXTI"),
        "LATENCY_BUDGET_US": (r.choice([30, 50, 80]), "us",
                              "Latency ISR -> task toi da cho phep"),
    }


def w3ex2(r):
    return {
        "DEBOUNCE_MS": (r.choice([15, 20, 25, 30]), "ms", "Cua so chong doi nut"),
        "LONGPRESS_MS": (r.choice([600, 800, 1000]), "ms", "Nguong nhan giu"),
        "TIMER_NAME_LEN": (r.choice([8, 12]), "bytes", "Do dai ten software timer"),
    }


def w3ex3(r):
    return {
        "STREAM_BUFFER_BYTES": (r.choice([64, 96, 128, 192]), "bytes",
                                "Kich thuoc stream buffer RX"),
        "TRIGGER_LEVEL": (r.choice([1, 4, 8, 16]), "bytes",
                          "Trigger level cua stream buffer"),
        "MAX_CMD_LEN": (r.choice([16, 24, 32]), "bytes", "Do dai lenh toi da"),
        "RX_BAUD": (r.choice([57600, 115200]), "baud", "Toc do USART1 cho RX"),
    }


def w3ex4(r):
    return {
        "STATS_WINDOW_MS": (r.choice([500, 1000, 2000]), "ms",
                            "Cua so tinh run-time stats bang DWT"),
        "IWDG_TIMEOUT_MS": (r.choice([250, 500, 1000]), "ms", "Timeout cua IWDG"),
        "SUPERVISOR_BITS": (r.choice([3, 4]), "bit",
                            "So task phai bao song qua event group"),
    }


def w4ex1(r):
    # Mini project W4.1-W4.4: ONE continuing project, not 4 independent
    # exercises -- the design doc (ex1), implementation (ex2), measurement
    # (ex3) and defense (ex4) must all see the SAME numbers. See
    # RNG_KEY_ALIAS below: ex2/ex3/ex4 reuse this function with the ex1 RNG
    # key so their app/params.h is identical to ex1's.
    sample_hz = r.choice([200, 250, 400, 500])
    return {
        "SAMPLE_RATE_HZ": (sample_hz, "Hz", "Tan so lay mau MPU6050 (bat buoc)"),
        "ISR_TO_PROC_DEADLINE_US": (r.choice([300, 500, 800]), "us",
                                    "Deadline cung tu ISR den khi xu ly xong mot mau"),
        "LOG_BLOCK_SAMPLES": (r.choice([16, 32, 64]), "mau",
                              "So mau moi khoi ghi ra UART"),
        "UART_BAUD": (r.choice([115200, 230400, 460800]), "baud", "Toc do USART1"),
        "MAX_HEAP_BYTES": (r.choice([10240, 11264, 12288]), "bytes",
                           "Heap toi da duoc phep dung"),
    }


# ex2 (hien thuc), ex3 (do & chung minh) va ex4 (bao ve) dung chung mot bo
# tham so voi ex1 -- xem ghi chu trong w4ex1.
w4ex2 = w4ex1
w4ex3 = w4ex1
w4ex4 = w4ex1


EXERCISES = {
    "week1/ex1": w1ex1, "week1/ex2": w1ex2, "week1/ex3": w1ex3, "week1/ex4": w1ex4,
    "week2/ex1": w2ex1, "week2/ex2": w2ex2, "week2/ex3": w2ex3, "week2/ex4": w2ex4,
    "week3/ex1": w3ex1, "week3/ex2": w3ex2, "week3/ex3": w3ex3, "week3/ex4": w3ex4,
    "week4/ex1": w4ex1, "week4/ex2": w4ex2, "week4/ex3": w4ex3, "week4/ex4": w4ex4,
}

# weekN/exM entries that must share ONE RNG draw with another entry (same
# continuing mini project) instead of getting their own independent params.
RNG_KEY_ALIAS = {
    "week4/ex2": "week4/ex1",
    "week4/ex3": "week4/ex1",
    "week4/ex4": "week4/ex1",
}


def guard(name: str, value):
    """Chan cung: khong bao gio sinh ra bo so khong chay duoc tren 20 KB RAM."""
    if name.endswith("_STACK_WORDS"):
        assert 64 <= value <= 256, f"{name}={value} ngoai khoang an toan"
    if name in ("HEAP_SIZE_BYTES", "MAX_HEAP_BYTES"):
        assert 4096 <= value <= 13312, f"{name}={value} ngoai khoang an toan"
    if name == "STREAM_BUFFER_BYTES":
        assert value <= 256, f"{name}={value} qua lon"


def emit(exdir: Path, student_id: str, key: str, params: dict) -> None:
    gname = "PARAMS_" + key.replace("/", "_").upper() + "_H"
    h = [
        "/* AUTO-GENERATED by tools/gen_params.py -- DO NOT EDIT BY HAND.",
        f" * student_id = {student_id}",
        f" * exercise   = {key}",
        " */",
        f"#ifndef {gname}",
        f"#define {gname}",
        "",
        f'#define STUDENT_ID "{student_id}"',
        "",
    ]
    md = [
        f"# Tham so rieng — {key}",
        "",
        f"`student_id`: **{student_id}**",
        "",
        "Bo so nay la cua rieng ban. Report dung so khac voi bang duoi day se bi tra lai.",
        "",
        "| Tham so | Gia tri | Don vi | Y nghia |",
        "|---|---|---|---|",
    ]
    for name, (value, unit, desc) in params.items():
        guard(name, value)
        h.append(f"#define {name:<28} ({value})" + (f"  /* {unit} */" if unit else ""))
        md.append(f"| `{name}` | {value} | {unit or '—'} | {desc} |")
    h += ["", f"#endif /* {gname} */", ""]
    md += ["", "Sinh lai bat ky luc nao bang:", "",
           "```bash", f"python3 tools/gen_params.py {student_id}", "```", ""]

    (exdir / "app").mkdir(parents=True, exist_ok=True)
    (exdir / "app" / "params.h").write_text("\n".join(h), encoding="utf-8")
    (exdir / "PARAMS.md").write_text("\n".join(md), encoding="utf-8")


def main() -> int:
    if len(sys.argv) != 2 or not sys.argv[1].strip():
        print(__doc__)
        return 2
    student_id = sys.argv[1].strip()

    written = []
    for key, fn in EXERCISES.items():
        exdir = ROOT / key
        if not exdir.is_dir():
            continue  # tuan chua phat hanh
        rng_key = RNG_KEY_ALIAS.get(key, key)
        emit(exdir, student_id, key, fn(rng_for(student_id, rng_key)))
        written.append(key)

    if not written:
        print("Khong tim thay bai nao. Chay lenh nay tu goc repo.")
        return 1
    for key in written:
        print(f"  {key}/app/params.h + {key}/PARAMS.md")
    print(f"Da sinh tham so cho student_id = {student_id} ({len(written)} bai).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
