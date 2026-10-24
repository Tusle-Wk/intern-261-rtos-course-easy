# Dự đoán — Week 2 / Ex1

> **Commit TRƯỚC khi nạp chương trình.**

`student_id`: 2413858

Tham số: `SAMPLE_PERIOD_MS`=5, `QUEUE_LENGTH`=6, `FILTER_WINDOW`=16, `UART_BAUD`=230400

1. Phép tính kích thước queue tối thiểu (dùng `SAMPLE_PERIOD_MS`, thời gian xử lý ước lượng của `filter_task` và `uart_task` ở `UART_BAUD`):

   ```
   Tốc độ sinh mẫu: T = 5 ms  ->  200 mẫu/s.

   --- UART (uart_task) ---
   Dòng in: "seq,ax,ay,az\r\n", tối đa 10+1+6+1+6+1+6+2 = 33 byte (ví dụ với seq 6 chữ số: 29 byte).
   1 byte (8N1) = 10 bit -> 10 / 230400 = 43.4 us.
   t_uart = 33 x 43.4 us = 1.43 ms   (ví dụ 29 byte: 29 x 43.4 us = 1.26 ms)
   t_uart < T = 5 ms  -> UART xử lý nhanh hơn tốc độ đến (tải ~29%).

   --- filter_task ---
   Trung bình trượt O(1) (running sum): ước lượng ~20 us tính toán + ~10 us
   đổi ngữ cảnh/queue  ->  t_filter ~ 0.05 ms << T.

   --- Độ trễ xấu nhất của người tiêu thụ ---
   queue_raw:      filter chạy ngay sau khi sensor gửi (ưu tiên cao hơn uart);
                   trễ xấu nhất ~ t_filter + I2C của sensor (~0.3 ms) ~ 0.4 ms
                   Số mẫu đến trong khoảng đó = ceil(0.4 / 5) = 1
   queue_filtered: trễ xấu nhất ~ t_uart + phần CPU bị sensor/filter/logger chiếm
                   ~ 1.43 + 0.5 ~ 2 ms  ->  ceil(2 / 5) = 1

   Quy tắc: N_min = ceil(t_trễ_xấu_nhất / T) = 1 cho mỗi queue.
   Cộng 1 phần tử dự phòng cho jitter (I2C, ngắt) -> khuyến nghị 2.
   Khả năng chịu "đứng hình" của consumer với QUEUE_LENGTH=6: 6 x 5 ms = 30 ms.
   ```

   Kích thước tối thiểu dự đoán: `queue_raw` = 1 (khuyến nghị 2), `queue_filtered` = 1 (khuyến nghị 2).
   So với `QUEUE_LENGTH` = 6: **đủ** (dư ~3x), vì cả 2 consumer đều xử lý 1 mẫu trong << 5 ms (1.43 ms ở tầng chậm nhất) nên backlog xấu nhất chỉ ~1 mẫu.

2. Khi hạ UART_BAUD xuống 9600 (tạm thời), queue nào sẽ đầy trước? Vì sao?
   Hạ `UART_BAUD` xuống 9600: 1 byte = 1.042 ms, 1 dòng 20–33 byte ≈ 21–34 ms (độ dài dòng tăng dần theo số chữ số của `seq`) >> 5 ms. UART phục vụ chỉ ~30–48 dòng/s trong khi đến 200 dòng/s. 
   **`queue_filtered` đầy trước**, vì uart_task là nút cổ chai và queue_filtered nằm ngay trước nó. 
   Dự đoán thời điểm đầy: ≈ 30–35 ms sau khi bắt đầu (với dòng 29–33 byte là ~QUEUE_LENGTH x T = 30 ms; với dòng ngắn ~20 byte lúc mới chạy là ~35 ms). Mẫu đầu tiên bị rớt (timeout 0) có seq ≈ 8 (có thể 9 tùy độ dài dòng). Sau đó phần lớn mẫu bị rớt (~75–85%). `queue_raw` chỉ đầy sau đó nếu filter_task bị chặn (xQueueSend với portMAX_DELAY).

3. Với xQueueSend(..., 0) (timeout = 0) khi queue đầy: điều gì xảy ra với mẫu đó?
   `xQueueSend(..., 0)` khi queue đầy: trả về `errQUEUE_FULL` ngay lập tức, mẫu đó bị **vứt bỏ** (không thử lại), task không bị chặn nên chu kỳ lấy mẫu giữ nguyên, mất mẫu thể hiện bằng "lỗ" trong seq. (Timeout hữu hạn, ví dụ 2 ms < T: task chờ tối đa 2 ms ở trạng thái Blocked, nếu vẫn không có chỗ thì trả về `errQUEUE_FULL` và mẫu bị bỏ như trên, nhưng chu kỳ chỉ bị trễ tối đa bằng timeout.)

4. Với xQueueSend(..., portMAX_DELAY) khi queue đầy: sensor_task sẽ ở trạng thái nào? Có nguy cơ gì cho chu kỳ lấy mẫu?
   `xQueueSend(..., portMAX_DELAY)` khi queue đầy: `sensor_task` chuyển sang trạng thái
   **Blocked** (chờ có chỗ trống), không tốn CPU; không mất mẫu nào ở chỗ gửi, nhưng
   **chu kỳ lấy mẫu bị phá**: task ngủ lâu hơn 5 ms, sau khi thức `vTaskDelayUntil` thấy đã trễ nên chạy bù liên tiếp -> mẫu không còn cách đều theo thời gian (seq vẫn liên tục, không có "lỗ"). Nếu uart kẹt vĩnh viễn thì sensor cũng kẹt vĩnh viễn.

5. Độ đầy queue_raw theo thời gian ở cấu hình bình thường sẽ trông như thế nào (tăng dần / dao động quanh một mức / luôn gần 0)? Vì sao?
   `queue_raw` ở cấu hình bình thường: **luôn gần 0** (thỉnh thoảng nhảy lên 1 rất ngắn) vì filter_task có ưu tiên cao và xử lý mỗi mẫu trong vài chục µs, nên gần như luôn lấy hết mẫu ngay khi sensor gửi; logger lấy mẫu mỗi 1 ms hầu như chỉ thấy 0.