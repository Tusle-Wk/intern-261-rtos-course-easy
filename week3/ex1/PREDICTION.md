# Dự đoán — Week 3 / Ex1

> **Commit TRƯỚC khi nạp chương trình.**

`student_id`: 2413858

1. Với `ISR_NVIC_PRIO` của bạn và nhóm priority `NVIC_PRIORITYGROUP_4`, số này
   nằm trong khoảng cho phép (`configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` ..
   `configLIBRARY_LOWEST_INTERRUPT_PRIORITY`) không? Trình bày phép so sánh.

   **Trả lời: Có, nằm trong khoảng cho phép.**

   Tham số của mình: `ISR_NVIC_PRIO = 7`. Với `NVIC_PRIORITYGROUP_4`, cả 4 bit là
   preemption priority (không có sub-priority), số càng nhỏ ưu tiên càng cao.
   Trong `FreeRTOSConfig.h`: `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 5`,
   `configLIBRARY_LOWEST_INTERRUPT_PRIORITY = 15`, `configPRIO_BITS = 4`.

   Phép so sánh ở dạng số thô:

       configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY <= ISR_NVIC_PRIO <= configLIBRARY_LOWEST_INTERRUPT_PRIORITY
                                                  5 <=      7       <=                 15      => hợp lệ

   Phép so sánh ở dạng giá trị thanh ghi (dịch trái `8 - configPRIO_BITS = 4` bit):

       configMAX_SYSCALL_INTERRUPT_PRIORITY = 5  << 4 = 0x50
       ISR_NVIC_PRIO ghi vào NVIC           = 7  << 4 = 0x70
       configKERNEL_INTERRUPT_PRIORITY      = 15 << 4 = 0xF0
                                 0x50 <= 0x70 <= 0xF0     => hợp lệ

   Vì 0x70 >= 0x50 nên ngắt EXTI bị BASEPRI (đặt 0x50 khi kernel vào critical
   section) che được, do đó an toàn khi gọi API `FromISR`.

2. Khi đặt priority EXTI cao hơn ngưỡng cho phép rồi gọi
   `vTaskNotifyGiveFromISR`, bạn dự đoán hành vi quan sát được là gì (reset?
   treo? thông báo gì trên UART/debugger)?

   **Trả lời: Chương trình treo, không reset, không có thông báo nào trên UART.**

   - Giá trị sai dùng để thử: 2, tức `2 << 4 = 0x20`, nhỏ hơn ngưỡng `0x50`
     (ưu tiên phần cứng cao hơn ngưỡng).
   - Ngay đầu `vTaskNotifyGiveFromISR`, macro
     `portASSERT_IF_INTERRUPT_PRIORITY_INVALID()` gọi
     `vPortValidateInterruptPriority()` trong `port.c`. Hàm này đòi
     `ucCurrentPriority >= ucMaxSysCallPriority`, tức `0x20 >= 0x50`, điều kiện
     sai nên `configASSERT` kích hoạt.
   - `configASSERT` của mình là
     `if((x)==0){taskDISABLE_INTERRUPTS(); for(;;);}`: tắt ngắt rồi lặp vô hạn,
     nên **không reset** và **không in gì ra UART** (mình không thêm hook in log).
   - Trên debugger: CPU dừng ở dòng `configASSERT` trong
     `vPortValidateInterruptPriority` (`port.c`); call stack đi từ
     `vTaskNotifyGiveFromISR` <- `exti_button_isr_handler` <-
     `HAL_GPIO_EXTI_Callback` <- `EXTI15_10_IRQHandler`.
   - Trên logic analyzer: PB11 có cạnh xuống nhưng PA8 không toggle.
   - Note: assert chỉ nổ ở lần nhấn đầu tiên sau khi `ack_task` đã chạy và gán
     `s_ack_task_handle` (khi handle còn NULL, ISR bỏ qua lệnh notify).

3. Dự đoán latency ISR → task (có `portYIELD_FROM_ISR`) theo đơn vị us, dựa
   trên tần số CPU 72 MHz và số lệnh ước lượng trong đường đi ISR → context
   switch. So với `LATENCY_BUDGET_US`: đạt hay không?

   **Trả lời: khoảng 10 us (trong khoảng 7 đến 15 us), pass so với `LATENCY_BUDGET_US = 50` us.**

   CPU 72 MHz nên 1 chu kỳ ≈ 13,9 ns; ngân sách 50 us tương đương khoảng 3600
   chu kỳ. Ước lượng số lệnh và chu kỳ từng đoạn của đường đi
   ISR → context switch → toggle (chu kỳ lớn hơn số lệnh vì flash có wait state,
   lệnh rẽ nhánh và load/store, CPI ước lượng khoảng 1,6):

   | Đoạn | Số lệnh ước lượng | Chu kỳ ước lượng |
   |---|---|---|
   | Vào ngắt (phần cứng cất thanh ghi, nạp vector) | — | 12 đến 20 |
   | EXTI handler → HAL → callback → `exti_button_isr_handler` | 30 đến 60 | 50 đến 100 |
   | `vTaskNotifyGiveFromISR` (kiểm tra priority, thao tác danh sách ready) | 100 đến 200 | 150 đến 300 |
   | `portYIELD_FROM_ISR` và thoát ISR sang PendSV | khoảng 5 | khoảng 20 |
   | PendSV (lưu ngữ cảnh, `vTaskSwitchContext`, nạp ngữ cảnh) | 120 đến 220 | 200 đến 350 |
   | Quay lại trong `ulTaskNotifyTake`, gọi `HAL_GPIO_TogglePin` | 60 đến 100 | 100 đến 150 |
   | **Tổng** | **khoảng 315 đến 585** | **khoảng 530 đến 940** |

   530 / 72 MHz ≈ 7,4 us; 940 / 72 MHz ≈ 13,1 us, nên dự đoán trung tâm khoảng
   10 us. So với `LATENCY_BUDGET_US = 50` us: **đạt**, dư địa gấp vài lần. Nếu
   build không tối ưu hoặc flash có wait state thì có thể lên cỡ 15 đến 20 us,
   vẫn dưới 50 us.

4. Nếu bỏ `portYIELD_FROM_ISR`, `ack_task` sẽ chạy **khi nào** (ngay lập tức
   hay phải chờ tick ngắt kế tiếp)? Latency tăng thêm cỡ bao nhiêu?

   **Trả lời: `ack_task` KHÔNG chạy ngay; phải chờ tick ngắt (SysTick) kế tiếp. Latency tăng thêm từ 0 đến khoảng 1000 us, trung bình khoảng 500 us.**

   - `vTaskNotifyGiveFromISR` vẫn đưa `ack_task` vào danh sách ready, nhưng không
     ai đặt cờ PendSV, nên CPU quay về task đang chạy (thường là Idle, priority
     task 0).
   - Đến khi SysTick kích hoạt, scheduler thấy `ack_task` (priority task 4) lớn
     hơn Idle (0) nên mới chuyển task.
   - `configTICK_RATE_HZ = 1000` nên một tick dài 1 ms. Thời điểm nhấn nằm ngẫu
     nhiên trong tick, nên latency tăng thêm phân bố đều từ 0 đến khoảng 1000 us.
   - Xác suất latency thêm nhỏ hơn khoảng 40 us (còn đạt 50 us) chỉ cỡ 4%, nên
     cấu hình này dự đoán **không đạt** `LATENCY_BUDGET_US` ở hầu hết các lần
     nhấn, và latency **dao động** giữa các lần nhấn (khác với cấu hình có yield
     luôn ổn định).