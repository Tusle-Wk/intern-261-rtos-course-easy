# Dự đoán — Week 1 / Ex2

> **Commit TRƯỚC khi nạp chương trình.**

`student_id`: 2413858    Ưu tiên của tôi: A=1 B=2 C=4

1. Bước 2 (`configUSE_PREEMPTION`=1, ưu tiên khác nhau): task nào chạy? task nào
   **không bao giờ** chạy? CPU share dự đoán:
   - Task chạy: Chỉ duy nhất task C chạy.
   - Task không bao giờ chạy: task A và task B (bị starvation hoàn toàn).

   | Task | CPU share dự đoán (%) |
   |---|---|
   | A | 0% |
   | B | 0% |
   | C | ~100 % (do trừ thời gian ngắt SysTick/TIM4 và lệnh hạ chân)|

2. Bước 3 (`configUSE_PREEMPTION`=0): dự đoán thay đổi gì so với bước 2? Vì sao?
- Dự đoán: Không thay đổi gì so với Bước 2 (Task C vẫn chiếm ~100% CPU, Task A và B vẫn 0%).
- Vì sao: Khi gọi `vTaskStartScheduler()`, FreeRTOS luôn chọn task có độ ưu tiên cao nhất trong danh sách Ready là task C để chạy đầu tiên. Khi `configUSE_PREEMPTION = 0`, kernel không tự động chuyển ngữ cảnh ở ngắt SysTick mà chỉ chuyển khi task đang chạy tự nguyện gọi API gây Block hoặc `taskYIELD()`. Vì Task C chạy vòng lặp vô tận không bao giờ block, nó vẫn độc chiếm CPU mãi mãi.
3. Bước 4 (3 task cùng ưu tiên): time slice dự đoán = 1 ms. Dựa vào đâu?
- Dựa vào `configTICK_RATE_HZ = 1000` (1 tick = 1 ms) kết hợp với cơ chế Round-Robin time slicing mặc định của FreeRTOS khi `configUSE_PREEMPTION = 1`. Mỗi khi ngắt SysTick xảy ra (mỗi 1 ms), scheduler sẽ chuyển ngữ cảnh sang task kế tiếp có cùng độ ưu tiên trong hàng đợi Ready. Ba task A, B, C sẽ chia đều CPU (~33.3% mỗi task).
4. Bước 5: tôi sẽ sửa starvation bằng cách: Thêm `vTaskDelay(1)` vào cuối mỗi vòng lặp của task (ngay sau lệnh hạ chân GPIO xuống mức LOW). Cách này đánh đổi gì?
- Lãng phí tài nguyên CPU: Tổng thời gian làm việc của C (120 us) + B (300 us) + A (240 us) = 660 us trong mỗi chu kỳ tick 1 ms (1000 us). Như vậy có ~340 us (34% CPU) bị bỏ trống cho Idle task chạy dù các task vẫn có nhu cầu xử lý tiếp.
- Tần suất thực thi của các task bị khóa cứng vào độ phân giải của SysTick (1 ms/lần) và phát sinh chi phí context switch overhead mỗi 1 ms.
5. Chân nào sẽ không bao giờ đổi trạng thái trên logic analyzer ở bước 2?
- Chân PA0 (Task A) và PA4 (Task B) sẽ luôn giữ mức LOW và không bao giờ đổi trạng thái.
