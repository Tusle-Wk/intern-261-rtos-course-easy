# Dự đoán — Week 4 / Ex1

> **Commit TRƯỚC khi gửi design doc cho mentor duyệt.**

`student_id`: 2413858

1. Với `SAMPLE_RATE_HZ` của bạn, chu kỳ lấy mẫu là bao nhiêu us? Dựa trên
   latency ISR→task thật bạn đã đo ở `week3/ex1`, dự đoán task xử lý có kịp
   `ISR_TO_PROC_DEADLINE_US` không? Trình bày phép tính, không đoán.

   **Trả lời:** Chu kỳ lấy mẫu = 1 / 500 Hz = **2000 us**. Deadline 500 us bằng 25% chu kỳ.
   Chuỗi từ lúc ISR nổ đến khi xử lý xong 1 mẫu (ISR timer → task lấy mẫu → queue → task xử lý):

   | Bước | Nguồn | us |
   |---|---|---|
   | ISR → task lấy mẫu thức dậy (có `portYIELD_FROM_ISR`) | đo thật week3/ex1: 14,500 us | 14,5 |
   | Task lấy mẫu đọc 14 byte MPU6050 qua I2C 400 kHz | tính: (9 + 9 + 9 + 14 × 9) = 153 bit + khoảng 3 bit start/stop = 156 bit / 400000 | 390 |
   | Task lấy mẫu → task xử lý thức dậy qua queue | lấy cận trên bằng số đo week3 (chưa đo riêng) | 14,5 |
   | Xử lý 1 mẫu (lọc 6 trục + ghi 16 byte vào stream buffer) | ước lượng khoảng 1440 chu kỳ ở 72 MHz, build `-O0` (đường ISR→task đã đo tốn 1044 chu kỳ) | 20 |
   | **Tổng** | 14,5 + 390 + 14,5 + 20 | **439** |

   439 us < 500 us, dư 61 us (khoảng 12%): **dự đoán kịp deadline, nhưng biên mỏng và phụ thuộc vào I2C**.
   Thiết kế yêu cầu I2C chạy 400 kHz; nếu chỉ chạy 100 kHz thì đọc I2C mất 1560 us > 500 us, không đạt.
   Bắt buộc có `portYIELD_FROM_ISR`: thiếu nó latency đo được là 541,871 us (> 500 us) và có thể lên khoảng 1 ms (tick 1 kHz).

2. Dự đoán tổng RAM (queue + stack mọi task) theo thiết kế của bạn, tính
   bằng bytes. So với `MAX_HEAP_BYTES`: còn dư bao nhiêu?

   **Trả lời:** Thiết kế: queue mẫu (64 phần tử) + stream buffer 2048 byte (trigger level 1024 byte = 1 khối 64 mẫu) + 3 task + idle task.
   Cấp phát động bằng `heap_4` (mỗi khối cộng header 8 byte, làm tròn bội số 8; TCB 76 byte, theo week1/ex4).
   Một mẫu `sensor_sample_t` = 4 (timestamp) + 6 × 2 (accel, gyro) = 16 byte.

   | Thành phần | Tính | Bytes |
   |---|---|---|
   | Queue mẫu | 64 × 16 = 1024, cộng control block 72 (theo `.map` week1), cộng header 8 | 1104 |
   | Stream buffer | 2048 + 1, cộng control block ước lượng 48, cộng header 8, làm tròn | 2112 |
   | Task lấy mẫu | TCB (76 + 8, làm tròn 88) + stack 128 words × 4 + 8 = 520 | 608 |
   | Task xử lý | như trên | 608 |
   | Task ghi UART | như trên | 608 |
   | Idle task (`configSUPPORT_STATIC_ALLOCATION = 0`, `configUSE_TIMERS = 0`) | như trên | 608 |
   | Căn lề đầu và cuối heap của `heap_4` | 4 + 12 (theo week1/ex4 mục 3) | 16 |
   | **Tổng** | 1104 + 2112 + 4 × 608 + 16 | **5664** |

   So với `MAX_HEAP_BYTES` = 10240: **còn dư 4576 byte** (khoảng 45%).
   Ước lượng stack 128 words mỗi task là cận trên: task đơn giản ở week1 chỉ dùng 25–39 words, riêng task có printf dùng khoảng 178 words.
   Stack task lấy mẫu có thể sâu hơn vì gọi HAL I2C; sẽ đo HWM ở ex3.

3. Nếu task xử lý tạm thời chậm hơn ISR lấy mẫu (ví dụ do một task khác
   chiếm CPU), hàng đợi của bạn đầy sau bao nhiêu mẫu? Điều gì xảy ra với
   mẫu tiếp theo trong thiết kế của bạn?

   **Trả lời:** Queue dài 64, mỗi 2 ms thêm 1 mẫu, nên nếu task xử lý đứng hẳn thì queue đầy sau **64 mẫu = 128 ms**.
   Mẫu thứ 65: `xQueueSend` với timeout 0 trả về `errQUEUE_FULL`, thiết kế **drop mẫu mới** (không chờ, không chặn task lấy mẫu) và tăng biến đếm `dropped_samples`.
   Biến đếm này dùng để báo cáo số mẫu rớt ở ex3.
   Chọn drop mẫu mới vì FreeRTOS không có thao tác "drop mẫu cũ" cho queue dài hơn 1 phần tử.

4. Bạn dự đoán priority nào (ISR task xử lý vs. task ghi UART) sẽ cho kết
   quả đạt deadline tốt hơn? Vì sao?

   **Trả lời:** Dự đoán **task xử lý ưu tiên cao hơn task ghi UART** (task lấy mẫu cao hơn nữa): lấy mẫu = 3, xử lý = 2, UART = 1, idle = 0 (`configMAX_PRIORITIES` = 5).
   Lý do: một khối 1024 byte ở 115200 baud mất khoảng 88,9 ms để truyền (1024 × 10 bit / 115200).
   Nếu UART ưu tiên cao hơn và truyền kiểu chờ (blocking), task xử lý có thể bị chặn đến khoảng 89 ms, vượt xa deadline 500 us (queue lúc đó giữ khoảng 44 mẫu, chưa đầy nhưng đã trễ).
   Khi task xử lý ưu tiên cao hơn, nó chiếm CPU từ task UART trong khoảng 14,5 us (latency đã đo), nên deadline giữ được.
   Băng thông: 500 × 16 = 8000 B/s so với khả năng 11520 B/s của UART (khoảng 69%), và 88,9 ms truyền < 128 ms gom đủ khối, nên UART theo kịp nếu ghi dạng binary.