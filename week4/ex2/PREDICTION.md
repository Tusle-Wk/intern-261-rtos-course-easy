# Dự đoán — Week 4 / Ex2

> **Commit TRƯỚC khi nạp chương trình.**

`student_id`: 2413858

1. Lỗi gọi API queue không phải `FromISR` bên trong ISR sẽ gây ra hiện
   tượng gì khi chạy (configASSERT? treo? reset?). Giải thích bằng cơ chế
   (ngăn xếp ISR, gọi `vTaskSuspendAll`/scheduler từ ngữ cảnh ngắt), không
   chỉ "nó bị lỗi".

   **Trả lời:** Dự đoán **`configASSERT` kích hoạt và chương trình treo** (không reset, không có đầu ra UART), ngay ở lần ngắt timer đầu tiên (sau khoảng 2 ms).
   Debugger dự kiến dừng trong `port.c`, ở `vPortEnterCritical` (kiểm tra đang ở trong ngắt), với call stack đi từ `xQueueGenericSend` ← `xQueueSend` ← `sampler_timer_callback` ← IRQ handler của timer.
   Cơ chế:
   - `xQueueSend` là API dành cho task: bên trong nó vào vùng găng bằng `taskENTER_CRITICAL()`, bản này giả định đang chạy trong ngữ cảnh task và có đếm lồng `uxCriticalNesting`; port dùng `configASSERT` để bắt trường hợp thanh ghi IPSR cho thấy đang ở trong ngắt. Bản `FromISR` thì dùng `taskENTER_CRITICAL_FROM_ISR()`, chỉ lưu và khôi phục `BASEPRI`.
   - Khi `configASSERT` sai, macro chạy `taskDISABLE_INTERRUPTS(); for( ;; );`: tắt ngắt rồi lặp vô hạn, nên hệ thống treo tại chỗ.
   - ISR chạy trên ngăn xếp chính (MSP), không phải stack của task nào. Nếu assert bị tắt và queue đầy với timeout khác 0, API task sẽ đi vào đường chờ (`vTaskSuspendAll`, đặt "task hiện tại" lên danh sách chờ): nhưng "task hiện tại" chỉ là task bị ngắt, không phải ISR, nên danh sách của scheduler bị sửa sai. Lúc thoát vùng găng nó còn đặt `BASEPRI = 0`, mở lại các ngắt mà ISR đang cần giữ chặn.
   - Bản `FromISR` không chạm vào trạng thái scheduler mà trả về `xHigherPriorityTaskWoken`, để `portYIELD_FROM_ISR` quyết định chuyển task khi thoát ISR.
   Điều kiện của dự đoán: `configASSERT` đang được định nghĩa trong `FreeRTOSConfig.h` (nếu bị tắt thì sẽ không có assert, và lỗi chỉ lộ ra dưới dạng hỏng ngầm).
   - Điều kiện thêm: ISR phải chạy tới được xQueueSend. Nếu hàm đọc cảm biến trong ISR bị kẹt trước đó (ví dụ HAL_I2C_Mem_Read chờ timeout khi MPU6050 chưa nối), chương trình sẽ treo trong HAL chứ không dừng ở assert.
   
2. Sau khi sửa đúng API `FromISR`, dự đoán số mẫu ghi UART mỗi giây =
   `SAMPLE_RATE_HZ` / `LOG_BLOCK_SAMPLES` khối/giây. Tính ra con số.

   **Trả lời:** 500 Hz / 64 mẫu = **7,8125 khối/giây**, tức cứ 64 × 2 ms = 128 ms có một khối (mỗi khối 64 × 16 = 1024 byte nếu ghi binary).

3. Với `UART_BAUD` của bạn, một khối `LOG_BLOCK_SAMPLES` mẫu mất bao lâu để
   truyền xong? Có nguy cơ UART chưa gửi xong khối cũ mà khối mới đã sẵn
   sàng không?

   **Trả lời:** Mỗi byte truyền mất 10 bit (1 start + 8 data + 1 stop, UART 8N1), nên 1024 byte mất 1024 × 10 / 115200 = **88,9 ms (88889 us)**.
   Khối mới sẵn sàng sau mỗi 128 ms, dư 128 − 88,9 = 39,1 ms (khoảng 30,6%), nên **không có nguy cơ** UART chưa gửi xong khối cũ khi ghi binary: UART chiếm khoảng 69% thời gian.
   Trong 88,9 ms đó vẫn có khoảng 44 mẫu mới đến (88,9 / 2); hàng đợi 64 mẫu đủ chứa, nên không mất mẫu.
   Nếu thêm header vài byte cho mỗi khối thì thời gian tăng không đáng kể (mỗi byte thêm mất khoảng 87 us).
   Nếu ghi dạng text thì không đạt: mỗi mẫu dài khoảng 60 ký tự, 500 mẫu/s vượt khả năng 11520 byte/s của UART.