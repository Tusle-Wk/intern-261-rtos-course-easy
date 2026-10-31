# Report — Week 2 / Ex2

`student_id`: 2413858    Commit của PREDICTION.md: e8875fb

## 1. Lỗi tái hiện (UART_GUARD_MODE = 0)
Log UART dán nguyên văn, đánh dấu vị trí ký tự lẫn lộn:

```
W0 #00000 aaaaaaaaaaaaW1 #00000 bbbbbbbbbbbbbW2 #00000 cccccccccccccaaaaaaaaaaaaaaaa
                      ^                      ^                      ^
bbbbccccccccccccccc
    ^
bbbbbbbbbbb
W0 #00001 aaaaaaaaaaaaaW2 #00001 cccccccccccccaaaaaaaaaaaaaaa
                       ^                      ^
W1 #ccccccccccccccc
    ^
00001 bbbbbbbbbbbbbbbbbbbbbbbbbbbb
^
```

`^` đánh dấu chỗ CPU bị chuyển sang writer khác giữa chừng một bản tin. Dòng 1: byte 22 (W0 → W1), byte 45 (W1 → W2), byte 68 (W2 → W0). Dòng 2: byte 4 (W1 → W2). Dòng 4: byte 23 (W0 → W2), byte 46 (W2 → W0). Dòng 5–6: phần đuôi của W2 chen vào giữa `W1 #` và `00001` của W1 (byte 4 ở dòng 5 và byte 0 ở dòng 6).

Tái hiện lại bao nhiêu lần / bao nhiêu lần thấy lỗi: 5 / 5.

- Mỗi lần: nạp Mode 0, bấm RESET, bắt log UART khoảng 5 giây (file `mode_0.1.txt` … `mode_0.5.txt`). Cả 5 lần log đều bắt đầu bằng `W0 #00000` và **dòng 1 đã lỗi**.
- Đếm theo chu kỳ (một chu kỳ là bộ W0, W1, W2 cùng số thứ tự): lần 1: 548/548 chu kỳ có ít nhất một bản tin hỏng, lần 2: 572/572, lần 3: 589/589, lần 4: 602/602, lần 5: 570/570. Không có chu kỳ nào ba bản tin còn nguyên vẹn. (Đếm theo dòng thì khoảng 69% dòng lỗi, vì bản tin bị cắt vụn nên một dòng có thể ghép nhiều mảnh.)
- Nguyên nhân (interleaving ở mức byte): ở 230400 baud mỗi byte mất 10 bit ≈ 43,4 µs, nên bản tin MSG_LEN = 40 byte mất ≈ 1740 µs, lớn hơn 1 tick (1 ms). Ba writer cùng ưu tiên nên time slicing chuyển CPU mỗi 1 ms; trong 1 ms UART gửi được ≈ 1000/43,4 ≈ 23 byte, đúng khớp với các lát 22–23 byte trong log. Ở Mode 0 mỗi writer ghi thẳng vào `USART1->DR` mà không có khóa, nên khi bị chuyển CPU giữa chừng, writer tiếp theo chen byte của nó vào cùng đường truyền và các byte của W0, W1, W2 xen kẽ nhau.

## 2. Bảng so sánh 3 cách
| Cách | Độ trễ in trung bình (us) | Độ trễ in max (us) | Ảnh hưởng task khác | RAM thêm (bytes) |
|---|---|---|---|---|
| Mutex | 1752 | 4390 | `probe_task` trễ tối đa 1003 µs (chuẩn 1000), 0 lần trễ (`g_probe_late` = 0), kernel không mất tick (`g_tick_lag_ms` = 0) | 80 |
| Critical section | 3613 | 5427 | `probe_task` trễ tối đa 1845 µs, 250 lần trễ/s (đúng 1 lần cho mỗi bản tin: 7632/7632), kernel mất 16,7% tick (5088 ms trong 30,5 s) | 0 |
| Gatekeeper | 1767 | 1794 | `probe_task` trễ tối đa 1009 µs, 0 lần trễ, kernel không mất tick | 1472 |

Cách đo độ trễ in (mô tả ngắn, kèm ảnh logic analyzer nếu dùng pin đánh dấu):
Dùng bộ đếm chu kỳ `DWT->CYCCNT` ở 72 MHz (1 µs = 72 chu kỳ), không dùng logic analyzer.
- Mode 1 và Mode 2: lấy `t0 = DWT->CYCCNT` ngay khi vào `uart_guard_send` (trước khi Take mutex hoặc vào critical section) và gọi `lat_record(DWT->CYCCNT - t0)` sau khi nhả mutex / thoát critical section. Độ trễ gồm cả thời gian chờ khóa và thời gian truyền 40 byte.
- Mode 3: writer đóng dấu `t_req` ngay trước khi bỏ bản tin vào queue; gatekeeper gọi `lat_record(DWT->CYCCNT - t_req)` sau khi in xong. Độ trễ gồm thời gian nằm trong queue và thời gian truyền.
- `lat_record` cộng vào `g_lat_sum_us`, cập nhật `g_lat_max_us` và tăng `g_lat_count` (trong một critical section rất ngắn); trung bình = `g_lat_sum_us / g_lat_count`.
- Ảnh hưởng task khác: `probe_task` (ưu tiên 4, chu kỳ 1 ms) đo khoảng cách thực giữa hai lần nó chạy (`g_probe_max_us`, `g_probe_late` tăng khi khoảng cách > 1500 µs); `g_tick_lag_ms` = thời gian thật (DWT) − thời gian tick của kernel, lớn hơn 0 nghĩa là mất tick. RAM thêm = `xPortGetFreeHeapSize()` trước trừ sau `uart_guard_init()`.
- Cách lấy số: chạy khoảng 30 s, Pause trong Cortex-Debug (SWD) và đọc các biến ở cửa sổ Watch trong cùng một lần dừng (DWT và tick dừng cùng nhau nên các số khớp nhau). Build Debug (-O0), cùng cấu hình cho cả ba mode, WRITER_COUNT = 3, 230400 baud. Mode 1 đo 2 lần (1752 / 4393 µs và 1752 / 4390 µs, lệch < 0,1%), bảng dùng lần 2; Mode 2 và Mode 3 đo 1 lần mỗi mode. Log UART của mỗi lần đo được lưu (`mode_1.txt`, `mode_2.txt`, `mode_3.txt`): cả ba có 0 dòng lỗi và số dòng đúng bằng `g_lat_count` (12162, 7632, 7570).

## 3. Vì sao critical section nguy hiểm nhất
Đoạn in trong critical section dài **1742 µs** (`g_cs_max_us` = 1742, lần gần nhất `g_cs_last_us` = 1738), và nó xảy ra 250 lần mỗi giây. Suốt đoạn đó `taskENTER_CRITICAL()` đặt BASEPRI = `configMAX_SYSCALL_INTERRUPT_PRIORITY` (5 << 4), nên **mọi ngắt có mức ưu tiên số ≥ 5 bị che hoàn toàn trong 1742 µs**, trong đó chắc chắn có SysTick và PendSV của kernel (ưu tiên 15); mọi ISR khác cùng dải ưu tiên (UART RX, timer, DMA… nếu có) cũng bị trễ tới 1,74 ms. Tính từ số đo, 250 × 1742 µs ≈ 43,5% thời gian hệ thống chạy với các ngắt đó bị che.

Hậu quả đo được:
- SysTick tick mỗi 1 ms, mà đoạn che dài 1,74 ms chứa 1–2 mốc tick. Cờ pending của SysTick chỉ giữ được 1 lần, nên khi đoạn che chứa 2 mốc thì mất 1 tick. Kết quả `g_tick_lag_ms` = 5088 ms sau 30,5 s thời gian thật (kernel chỉ đếm 25,4 s): đồng hồ kernel chậm **16,7%**, tức ≈ 0,67 tick mất cho mỗi bản tin. Mọi `vTaskDelay` vì thế dài hơn khoảng 20% so với thời gian thật.
- `probe_task` (ưu tiên cao nhất, chu kỳ 1 ms) bị chặn: khoảng cách lớn nhất giữa hai lần chạy `g_probe_max_us` = 1845 µs (so với 1003 µs ở mutex), và `g_probe_late` = 7632, đúng bằng số bản tin: mỗi bản tin in làm `probe_task` trễ đúng một lần.
- Với 6 writer tình trạng nặng hơn: kernel mất 23,8% tick (7195 ms / 30,2 s) và `probe_task` trễ 362 lần/s.

Mutex và gatekeeper không che ngắt: `probe_max` ≈ 1003–1009 µs, 0 lần trễ, 0 tick mất. Mutex chỉ làm các writer chờ nhau; gatekeeper chỉ làm gatekeeper bận. Độ trễ in của critical section cũng cao nhất (3613 µs trung bình so với 1752 và 1767 µs). Critical section là cách duy nhất làm hỏng cả những phần của hệ thống không liên quan tới việc in, và mức hỏng tỉ lệ với thời gian in (`MSG_LEN` / baud): bản tin dài hơn hoặc baud thấp hơn sẽ che ngắt lâu hơn tương ứng.

## 4. Lệch so với dự đoán
Không chỉ số định lượng nào lệch quá 15%. Các chỗ có lệch, kèm nguyên nhân:

| Mục | Dự đoán | Đo | Lệch / nguyên nhân |
|---|---|---|---|
| Q1: kích thước lát lẫn | ≈ 23 byte | 22, 23, 23 byte (byte 22, 45, 68) | −1 byte ở lát đầu: 1 ms chứa 1000/43,4 ≈ 23 byte, writer bắt đầu in trễ vài chục µs sau mốc tick nên lát đầu chỉ được 22 byte. |
| Q1: phần đuôi | Phần còn lại của W0, W1, W2 được in liền nhau theo thứ tự | W0 in nốt 18 byte, nhưng W1 chỉ in thêm 4 byte (`bbbb`) rồi W2 chen vào (15 `c`), sau đó W1 mới in nốt 11 byte | Các lát chia theo lưới tick 1 ms cố định, không theo ranh giới bản tin: phần đuôi của W0 (18 byte ≈ 0,78 ms) dùng gần hết lát, chỉ còn ≈ 0,2 ms cho W1 (4–5 byte), nên đuôi của W1 và W2 vẫn lẫn nhau. |
| Q1: các chu kỳ sau | ≥ 90% chu kỳ có lỗi | 100% chu kỳ có lỗi (5/5 lần) | Đúng dự đoán, nhưng ở trạng thái ổn định thường chỉ 2 trong 3 bản tin của mỗi chu kỳ bị lẫn (ví dụ `W2 #00333` còn nguyên, `W1 #00333` lẫn với `W0 #00334`). Suy ra từ log: ba writer dần lệch pha nên chỉ hai writer gần nhau còn chồng lên nhau. |
| Q2: trung bình | mutex ≈ 1750, gatekeeper ≈ 1780, critical section ≈ 3500 µs | 1752, 1767, 3613 µs | +0,1%, −0,7%, +3,2%. Thứ tự mutex ≲ gatekeeper < critical section đúng. Phần dư của critical section (≈ 110 µs) là chi phí đổi ngữ cảnh và làm tròn theo tick (ước tính). |
| Q2: max | gatekeeper ≈ 1800, mutex ≈ 4200, critical section ≈ 5200 µs | 1794, 4390, 5427 µs | −0,3%, +4,5%, +4,4%. Phần dư ≈ 0,2 ms của mutex và critical section là chi phí đổi ngữ cảnh và làm tròn tick cộng dồn qua hai lần chờ ở chu kỳ đầu (ước tính, không đo riêng). |
| Q3: `g_probe_max_us` (critical section) | ≈ 1,75–1,8 ms | 1845 µs | Cao hơn khoảng 2,5% so với cận trên: 1742 µs che ngắt cộng ≈ 100 µs để xử lý SysTick/PendSV tồn đọng và chuyển ngữ cảnh ngay sau khi thoát (ước tính). |
| Q3: `g_probe_late`, lag | ≈ 1 lần trễ mỗi bản tin, kernel chậm ≈ 17% | 7632 lần = 1 lần mỗi bản tin, 16,7% | 0%, đúng dự đoán. |
| Q4: RAM | mutex ≈ 80–90 B, gatekeeper ≈ 1,4–1,5 kB (≈ 17 lần) | 80 B, 1472 B (≈ 18,4 lần) | Trong khoảng dự đoán. Với 6 writer gatekeeper tăng đúng +264 B (1472 → 1736) = 6 ô queue thêm × 44 B, khớp với `GATEKEEPER_QUEUE_LEN` = 2 × `WRITER_COUNT`. |
| Q5: thứ tự tăng | Critical section tăng nhiều nhất, mutex vừa | Trung bình: mutex +2649 µs (×2,5), critical section +2382 µs (×1,66); max: critical section +5489 µs, mutex +5408 µs (≈ bằng nhau) | Sai thứ tự ở độ trễ trung bình. Giá trị tuyệt đối critical section vẫn cao nhất (5995 µs, max 10916 µs). |
| Q5: mutex 6 writer | ≈ 4 ms (max ≈ 9,4 ms) | 4401 µs (max 9798 µs) | +10% / +4%. Mô hình giả định UART bão hòa 100% (chu kỳ 6 × 1,74 = 10,44 ms). Thực tế 547 bản tin/s ⇒ UART chạy 95%: mỗi lần chuyển mutex từ writer này sang writer khác UART nghỉ ≈ 85 µs (1/547 s − 1,74 ms) do đổi ngữ cảnh, nên chu kỳ mỗi writer ≈ 6/547 ≈ 10,96 ms; trừ thời gian nghỉ thực sau `vTaskDelay(7)` ≈ 6,5 ms (như trong dự đoán) còn độ trễ ≈ 4,4 ms (giá trị tính từ số đo). |
| Q5: critical section 6 writer | ≈ 6,1 ms (max ≈ 10,4 ms), kernel chậm ≈ 23% | 5995 µs (max 10916 µs), 23,8% | −1,7% / +5%, đúng dự đoán. |
| Q5: gatekeeper 6 writer | ≈ 1,8 ms, chiếm ≈ 60% CPU | 1767 µs (max 1793 µs) | Độ trễ đúng dự đoán (−2% / 0%). Mức chiếm CPU không đo trực tiếp (không có bộ đếm idle) mà tính gián tiếp: gatekeeper in kiểu polling nên bận suốt 1742 µs mỗi bản tin, 333 bản tin/s × 1,742 ms ≈ 58% (3 writer: 250 × 1,742 ms ≈ 44%), khớp dự đoán ≈ 60% (−3%). |

Ngoài ra có một lỗi ở phép đo (không phải lệch dự đoán): ở lần đo Mode 1 đầu tiên `g_guard_heap_bytes` ra 4294958176. Nguyên nhân là `heap_4` chỉ khởi tạo heap ở lần `pvPortMalloc` đầu tiên, nên `xPortGetFreeHeapSize()` đọc trước `uart_guard_init()` trả 0 và phép trừ bị tràn số. Đã sửa bằng một lần `vPortFree(pvPortMalloc(1))` trước khi đọc `heap_before`; sau sửa Mode 1 đo được 80 B. Thay đổi này chỉ chạy một lần trước khi các task chạy nên không ảnh hưởng số đo thời gian.

## 5. Câu hỏi đánh đổi
`WRITER_COUNT` của tôi là 3, tăng gấp đôi thành 6 (heap tăng lên 12288 để đủ chỗ). Đo mỗi mode khoảng 30 s:

| | 3 writer (TB / max, µs) | 6 writer (TB / max, µs) | Tăng TB | Tăng max |
|---|---|---|---|---|
| Mutex | 1752 / 4390 | 4401 / 9798 | +2649 (×2,5) | +5408 (×2,2) |
| Critical section | 3613 / 5427 | 5995 / 10916 | +2382 (×1,66) | +5489 (×2,0) |
| Gatekeeper | 1767 / 1794 | 1767 / 1793 | 0 (×1,0) | −1 (×1,0) |

Ảnh hưởng khác khi lên 6 writer: critical section làm kernel mất 23,8% tick (từ 16,7%) và `probe_task` trễ 362 lần/s (từ 250); mutex và gatekeeper vẫn 0 lần trễ, 0 tick mất; RAM mutex vẫn 80 B, critical section 0 B, gatekeeper tăng từ 1472 lên 1736 B.

Trả lời: **về độ trễ trung bình thì mutex bị ảnh hưởng nhiều nhất** (×2,5, +2649 µs). Lý do: với 3 writer các writer tự lệch pha nên hầu như không phải chờ nhau (độ trễ ≈ chỉ thời gian truyền 1,74 ms); với 6 writer UART gần bão hòa (547 bản tin/s × 1,74 ms ≈ 95% thời gian) nên các writer phải xếp hàng chờ mutex. Về độ trễ max, critical section và mutex tăng gần bằng nhau (+5489 và +5408 µs, khoảng ×2). Critical section vẫn là cách có độ trễ tuyệt đối lớn nhất (5995 µs trung bình, 10916 µs max) và là cách duy nhất làm hỏng hệ thống ngoài các writer (tick và `probe_task`). Gatekeeper không đổi vì writer chỉ bỏ bản tin vào queue và quay đi, việc in tuần tự do một task duy nhất; cái giá là RAM (1472 → 1736 B). Dự đoán trước khi đo là critical section tăng nhiều nhất về độ trễ trung bình; kết quả đo cho thấy mutex tăng hơn một chút (xem mục 4).