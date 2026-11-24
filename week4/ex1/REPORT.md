# Report — Week 4 / Ex1 (Design doc)

`student_id`: 2413858    Commit của PREDICTION.md: 98a83d8

> File này ĐÓNG VAI TRÒ design doc 2–3 trang nộp cho mentor duyệt trước khi
> viết code. Điền đủ số liệu — không mô tả chung chung.

## 1. Task diagram
```
 TIM3 (500 Hz, PSC=7199, ARR=19)
   │  ISR, NVIC priority 7 (hợp lệ: 5..15)
   │  đọc DWT->CYCCNT, xTaskNotifyFromISR(giá trị = timestamp) + portYIELD_FROM_ISR
   ▼
 [T_sample, priority 3]  chờ notification; đọc 14 byte MPU6050 qua I2C 400 kHz;
   │                     gói sensor_sample_t (16 B); xQueueSend(timeout 0)
   ▼
 [sample_q: 64 × 16 B]
   ▼
 [T_proc, priority 2]    xQueueReceive; lọc IIR bậc 1 cho 6 trục;
   │                     xStreamBufferSend 16 B
   ▼
 [stream buffer: 2048 B, trigger level 1024 B = 64 mẫu]
   ▼
 [T_uart, priority 1]    xStreamBufferReceive 1024 B vào khối đệm (block); HAL_UART_Transmit
   ▼
 USART1 115200 baud
 Idle task: priority 0
```
Lý do priority: T_sample cao nhất vì nó phải chạy ngay khi ISR báo, đọc I2C đúng nhịp và đây là đoạn dài nhất của chuỗi deadline (390 us).
T_proc cao hơn T_uart vì deadline 500 us áp cho xử lý mẫu, còn T_uart truyền một khối mất khoảng 88,9 ms (1024 × 10 bit / 115200): nếu UART cao hơn, T_proc có thể bị chặn đến khoảng 89 ms.
Số priority nằm trong `configMAX_PRIORITIES` = 5 (0..4). TIM3 được chọn vì TIM4 đang là timebase của HAL (week1/ex4 mục 5).

## 2. Timing budget
| Đại lượng | Công thức / nguồn số liệu | Giá trị |
|---|---|---|
| Chu kỳ lấy mẫu | 1 / `SAMPLE_RATE_HZ` = 1 / 500 Hz | 2000 us |
| Latency ISR→task (đo thật, week3/ex1) | `week3/ex1/REPORT.md` mục 2: có `portYIELD_FROM_ISR`, 1 lần đo bằng logic analyzer | 14,5 us |
| Thời gian xử lý 1 mẫu (ước lượng): đọc I2C 14 byte | (9 + 9 + 9 + 14 × 9) = 153 bit + khoảng 3 bit start/stop = 156 bit / 400 kHz | 390 us |
| Thời gian xử lý 1 mẫu (ước lượng): T_sample → T_proc thức dậy | cận trên bằng số đo ISR→task (chưa đo riêng) | 14,5 us |
| Thời gian xử lý 1 mẫu (ước lượng): lọc + ghi stream buffer | khoảng 1440 chu kỳ ở 72 MHz (đường ISR→task đo được 1044 chu kỳ, `-O0`) | 20 us |
| Tổng so với `ISR_TO_PROC_DEADLINE_US` | 14,5 + 390 + 14,5 + 20 = 439 us so với 500 us, dư 61 us (12%) | **Đạt** |

Điều kiện để đạt:
- Phải có `portYIELD_FROM_ISR`: thiếu nó latency đo được là 541,871 us (> 500 us), có thể lên khoảng 1 ms vì tick 1 kHz.
- I2C phải chạy 400 kHz: ở 100 kHz đọc I2C mất 1560 us, không đạt.

Lưu ý: số 14,5 us là 1 lần đo, đo bằng task notification, build `-O0`. Hai số còn lại (14,5 us thứ hai và 20 us xử lý) là ước lượng, ex3 sẽ đo lại trên đúng đường đi của project. 390 us chỉ là thời gian trên dây I2C; phần mềm HAL_I2C_Mem_Read ở build -O0 chưa được tính, nên biên 61 us thực tế mỏng hơn và sẽ được đo ở ex3.

## 3. RAM budget
Cấp phát động bằng `heap_4`: mỗi khối cộng header 8 B, làm tròn bội số 8, TCB 76 B (88 B sau khi cộng header và làm tròn), số liệu theo `week1/ex4/REPORT.md` mục 3. Giả định `FreeRTOSConfig.h` của ex2 giống bản hiện tại: `configUSE_TIMERS = 0`, `configSUPPORT_STATIC_ALLOCATION = 0`, nên idle task nằm trong heap và không có timer task.

| Thành phần | Kích thước | Ghi chú |
|---|---|---|
| Queue (độ dài × cỡ phần tử) | 1104 B | 64 × 16 = 1024, cộng control block 72 (`xStaticTimerQueue` 72 trong `.map` week1), cộng header 8 |
| Stack task lấy mẫu | 608 B | stack 128 words × 4 = 512, cộng header 8 = 520, cộng TCB 88 |
| Stack task xử lý | 608 B | như trên |
| Stack task ghi UART | 608 B | như trên |
| Stream buffer (T_proc → T_uart) | 2112 B | 2048 + 1, cộng control block ước lượng 48, cộng header 8, làm tròn |
| Khối đệm gửi UART (T_uart) | 1032 B | mảng 1024 B cấp từ heap (`xStreamBufferReceive` copy một khối vào đây, `HAL_UART_Transmit` gửi từ mảng này), cộng header 8 |
| Idle task | 608 B | như trên |
| Căn lề đầu và cuối heap | 16 B | 4 + 12, `week1/ex4` mục 3 |
| **Tổng** | **6696 B** | So với `MAX_HEAP_BYTES` = 10240: **dư 3544 B** (còn 34,6%) |

Khối đệm gửi UART cần thiết vì stack T_uart chỉ có 512 B, không chứa nổi một khối 1024 B; `xStreamBufferReceive` copy dữ liệu ra một vùng nhớ liên tục và `HAL_UART_Transmit` cũng cần mảng liên tục. PREDICTION.md (5664 B) chưa tính khoản này, đây là chỗ lệch so với dự đoán, sẽ ghi nhận ở ex3.
Nguồn stack 128 words: ở week1/ex4 các task đơn giản chỉ dùng 25–39 words (biên an toàn đã chọn là 32 words), riêng `ram_report_task` có printf dùng khoảng 178 words. 128 words là cận trên cho cả ba task; HWM thật sẽ đo ở ex3.
`sizeof(sensor_sample_t)` = 16 B (4 B timestamp + 6 × 2 B), không có padding.

## 4. Lựa chọn API đồng bộ
- **Queue** cho đoạn T_sample → T_proc: mỗi mẫu là một struct 16 B cố định, cần bộ đệm FIFO 64 mẫu để chịu được lúc T_proc chạy chậm (128 ms), và `xQueueSend` với timeout 0 cho phép phát hiện đầy rồi drop (mục 5).
- **Stream buffer** cho đoạn T_proc → T_uart: dữ liệu là luồng byte và UART cần đúng khối 64 mẫu = 1024 byte. Đặt trigger level 1024 byte thì T_uart ngủ cho đến khi có đủ một khối, tức khoảng 7,8 lần thức dậy mỗi giây. Nếu dùng queue 16 B thì T_uart bị đánh thức 500 lần mỗi giây và phải tự gom khối. Stream buffer cho phép 1 writer và 1 reader, đúng với T_proc và T_uart.
- **Task notification** cho ISR → T_sample: nhẹ nhất và là đúng cơ chế đã đo 14,5 us ở week3/ex1. Notification mang luôn giá trị timestamp (DWT) lấy trong ISR.
- **Event group** không chọn: nó chỉ mang cờ, không mang dữ liệu mẫu, nên không thay được queue hay stream buffer ở đây.

## 5. Xử lý khi hàng đợi đầy
- Nếu T_proc đứng hẳn, queue 64 mẫu đầy sau 64 × 2 ms = **128 ms**.
- Mẫu thứ 65: `xQueueSend(q, &s, 0)` trả về `errQUEUE_FULL`, thiết kế **drop mẫu mới**, T_sample không chờ (không làm trễ nhịp lấy mẫu), và tăng biến đếm `dropped_samples`. Biến đếm này **có đo được**: báo cáo ở ex3.
- Chọn drop mẫu mới vì FreeRTOS không có thao tác drop mẫu cũ cho queue dài hơn 1 phần tử. Chỗ mất mẫu vẫn nhận ra qua timestamp: delta bình thường là 144000 chu kỳ (72 MHz / 500 Hz).
- Stream buffer đầy (dung lượng 2048 B = 128 mẫu = 256 ms): T_proc kiểm `xStreamBufferSpacesAvailable` >= 16 trước khi ghi, nếu không đủ thì drop mẫu đó và cộng vào cùng biến đếm, để không ghi nửa mẫu làm lệch các khối sau.
- Rủi ro tải CPU: I2C (390 us mỗi 2 ms, 19,5%) và UART (khoảng 88,9 ms mỗi 128 ms, 69,5%) đều truyền kiểu chờ, tổng khoảng 90% CPU. Deadline vẫn giữ được vì T_sample và T_proc ưu tiên cao hơn T_uart, nhưng nếu ex3 đo thấy `dropped_samples` > 0 thì chuyển UART sang DMA.

## 6. Mentor duyệt
Chữ ký / ghi chú của mentor sau buổi duyệt thiết kế: ______