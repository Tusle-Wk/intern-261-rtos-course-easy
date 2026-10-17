# Week 1 — Ex2: Ưu tiên, preemption và starvation

**Thời lượng dự kiến: 4 giờ**

## Mục tiêu
Nhìn thấy preemption xảy ra **trên logic analyzer**, không phải trên lý thuyết.
Tự tạo ra starvation, đo được nó, rồi sửa.

## Đọc trước
- *Mastering the FreeRTOS Real Time Kernel* — Ch.3 mục 3.6 (Task Priorities), 3.7 (Scheduling Algorithms)
- PM0056 — mục 4.4.5 System Handler Priority Registers

## Việc cần làm
1. Tạo 3 task A/B/C với ưu tiên `TASK_A_PRIO` / `TASK_B_PRIO` / `TASK_C_PRIO`.
   Mỗi task: kéo chân GPIO của nó lên, busy-loop dùng `TASK_x_WORK_US`, hạ chân xuống,
   rồi **không block** (vòng lặp vô tận thuần túy).
   - Chân: A = `PA<TASK_A_PIN>`, B = `PA<TASK_B_PIN>`, C = `PA<TASK_C_PIN>`.
   - Busy-loop phải đo bằng DWT cycle counter, không dùng `HAL_Delay`.
2. Bật 3 kênh logic analyzer cùng lúc. Chụp lại.
   **Ghi lại task nào không bao giờ chạy** — đó là starvation bạn vừa tạo ra.
3. Đổi `configUSE_PREEMPTION` = 0, nạp lại, chụp lại. So sánh.
4. Đổi 3 task về **cùng một mức ưu tiên** = `TASK_A_PRIO`, bật lại
   `configUSE_PREEMPTION` = 1, chụp lại. Đo time slice thực tế bằng logic analyzer.
5. Sửa starvation ở bước 2 **bằng cách ít xâm lấn nhất** mà vẫn giữ nguyên ưu tiên.
   Chụp lại để chứng minh đã sửa.

## Cần nộp
- `app/app_main.c` + `app/tasks.c`, `app/dwt.c` (đo chu kỳ CPU).
- 4 ảnh PulseView: bước 2, 3, 4, 5. Mỗi ảnh phải thấy đủ 3 kênh.
- `REPORT.md` với bảng CPU share đo được của từng task trong cả 4 cấu hình.
- `PREDICTION.md` commit trước khi nạp.

## Đạt yêu cầu khi
- Chỉ ra đúng trên ảnh **bước 4** thời điểm preemption xảy ra, với dấu thời gian;
  và giải thích được vì sao ảnh **bước 2** không có preemption nào (starvation).
- Time slice đo được ở bước 4 khớp với `configTICK_RATE_HZ` (sai số < 5%).
- Giải thích được tại sao cách sửa ở bước 5 hoạt động, và nói được cách sửa đó
  **đã đánh đổi gì**.
