# Dự đoán — Week 1 / Ex1

> **Commit file này TRƯỚC khi nạp chương trình lần đầu.** Report sẽ phải giải thích
> mọi chỗ dự đoán lệch so đo. Dự đoán sai không bị trừ điểm; dự đoán sau khi đo thì bị.

`student_id`: 2413858    Ngày/giờ commit: 10/10/2026 / 9:48 PM

1. Chu kỳ LED tôi sẽ đo được: 113 ms (`BLINK_PERIOD_MS` = 113)
2. Sai số giữa chu kỳ đo được và `BLINK_PERIOD_MS`: khoảng 1-5 us. Vì sao? Do thời gian vi điều khiển thực thi các lệnh C, ví dụ như tính toán, gọi hàm vTaskDelay, context switch của RTOS,... và sai số vật lý của bộ dao động thạch anh.
3. Nếu để HAL timebase ở lại SysTick, điều gì sẽ xảy ra khi `vTaskDelay()` được gọi?
   (chọn và giải thích: chạy bình thường / treo / tick sai nhịp / hard fault). Chọn treo, vì FreeRTOS dựa vào ngắt SysTick để tăng biến đếm thời gian (tick). Nếu HAL giữ SysTick mà FreeRTOS không chiếm được, biến đếm tick của kernel sẽ không bao giờ tăng, dẫn đến hàm vTaskDelay() sẽ block task chờ mãi mãi.
4. `BRINGUP_STACK_WORDS` = 96 words = 384 bytes. Task này có đủ không? Vì sao? Đủ, vì task này rất đơn giản, chỉ chứa vòng lặp vô tận thực hiện toggle GPIO và gọi API delay, không khai báo các biến cục bộ mảng lớn hay gọi các hàm lồng nhau sâu.
5. Trước khi map handler, linker sẽ báo lỗi gì (đoạn nguyên văn thông báo):
   multiple definition of 'SysTick_Handler'
   multiple definition of 'SVC_Handler'
   multiple definition of 'PendSV_Handler'
