# Report — Week 4 / Ex2

`student_id`: 2413858    Commit của PREDICTION.md: e4ec544

## 1. Mô tả lỗi non-FromISR đã sửa
Vị trí trong code, thông điệp/hành vi quan sát được trước khi sửa, và giải
thích chính xác vì sao đó là lỗi (tên macro assert nếu có, cơ chế kernel).
- Vị trí: `sampler_isr.c` (file gốc), hàm `sampler_timer_callback()` gọi từ `TIM3_IRQHandler`, dòng 43: `xQueueSend(sample_queue, &s, 0);`.
- Hành vi trước khi sửa: chương trình đứng ngay sau ngắt TIM3 đầu tiên, không có thông báo nào và UART không ra byte nào. Pause bằng debugger thì dừng ở `port.c:439`, trong `vPortEnterCritical()`, tại `configASSERT(( portNVIC_INT_CTRL_REG & portVECTACTIVE_MASK ) == 0)`. Call stack: `vPortEnterCritical` ← `xQueueGenericSend` ← `sampler_timer_callback` ← `TIM3_IRQHandler` ← `prvIdleTask`.
- Giải thích: `xQueueSend` là API dành cho task, bên trong dùng `taskENTER_CRITICAL()` tức `vPortEnterCritical()`. Hàm này không được gọi trong ngắt nên `configASSERT` kiểm tra `VECTACTIVE` phải bằng 0 (đang ở ngữ cảnh task). Trong ISR `VECTACTIVE` khác 0 nên assert thất bại, và `configASSERT` trong `FreeRTOSConfig.h` gọi `taskDISABLE_INTERRUPTS()` rồi lặp `for( ;; )` nên máy treo. Trong ISR chỉ được dùng API kết thúc bằng `FromISR`.
- Cách sửa: ISR chỉ gọi `xTaskNotifyFromISR(sampler_task_handle, DWT->CYCCNT, eSetValueWithOverwrite, &xHigherPriorityTaskWoken)` rồi `portYIELD_FROM_ISR(xHigherPriorityTaskWoken)`. `sampler_task` đọc MPU6050 và gọi `xQueueSend` ở ngữ cảnh task.

## 2. Kiến trúc hiện thực
So với design doc `week4/ex1/REPORT.md`: có điểm nào phải đổi so với thiết
kế ban đầu không? Vì sao?
- Không đổi kiến trúc: TIM3 (500 Hz) → ISR `xTaskNotifyFromISR` → `sampler_task` (ưu tiên 3) đọc MPU6050 → `sample_queue` (64 phần tử × 16 B) → `logger_task` (ưu tiên 2) lọc → stream buffer 2048 B (trigger 1024 B) → `uart_task` (ưu tiên 1) gom đủ 1024 B (64 mẫu) rồi `HAL_UART_Transmit` một lần.
- Chi tiết hiện thực bổ sung: `uart_task` lặp `xStreamBufferReceive` đến khi đủ 1024 B, vì hàm này chỉ block khi buffer rỗng và trả về ngay số byte đang có.

## 3. Thông lượng UART
| Đại lượng | Dự đoán | Đo được |
|---|---|---|
| Khối/giây | 7,8125 (500 mẫu/s ÷ 64 mẫu/khối) | 7,8372 (240.797 B ÷ 1024 ÷ 30 s) |
| Thời gian truyền 1 khối (us) | 88.889 (1024 B × 10 bit ÷ 115200) | ≈ 107.830 (DWT: 7.763.513 / 7.763.581 / 7.763.815 chu kỳ ÷ 72) |

## 4. Lệch so với dự đoán
Mỗi mục lệch + nguyên nhân.
- Thời gian truyền 1 khối: dự đoán 88.889 us, đo ≈ 107.830 us (+21%). Nguyên nhân (suy luận từ số liệu): `HAL_UART_Transmit` chờ bằng vòng lặp và `uart_task` có ưu tiên thấp nhất; mỗi 2 ms `sampler_task` (ưu tiên cao hơn, đọc I2C) chiếm CPU khoảng 0,5 ms (ước tính), dài hơn thời gian đệm của hai thanh ghi UART (khoảng 174 us), nên đường truyền bị trống từng đoạn. Khối vẫn kết thúc trước chu kỳ 128 ms nên không mất mẫu (0 mẫu mất trong 30 s).
- Khối/giây: 7,8372 so với 7,8125 (+0,3%) do biên cửa sổ bắt 30 s (thu thêm tối đa một khối); theo timestamp (15.048 khoảng cách đều 2000,0 us) đúng 7,8125.

## 5. AI_LOG.md
Xác nhận file tồn tại và đầy đủ: Có (`week4/ex2/AI_LOG.md`, đã liệt kê các prompt đã dùng).