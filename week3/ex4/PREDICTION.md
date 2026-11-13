# Dự đoán — Week 3 / Ex4

> **Commit TRƯỚC khi nạp chương trình.**

`student_id`: 2413858

1. Chạy `app/deadlock_demo.c` y nguyên như được cấp. Dự đoán: hệ thống sẽ
   treo sau khoảng bao lâu (ngay lập tức, hay sau vài chu kỳ)? Vì sao không
   phải ngay lập tức (gợi ý: cần cả 2 task "đua" vào đúng thời điểm)?  
   Trả lời: Treo gần như ngay trong chu kỳ đầu tiên, khoảng 5 ms sau khi scheduler chạy (không phải ở t = 0, cũng không phải sau nhiều chu kỳ). Không treo ở t = 0 vì deadlock chỉ hình thành khi cả hai task đã giữ mutex thứ nhất rồi mới xin mutex thứ hai: alpha giữ A, beta giữ B, đến khi cả hai thức dậy sau `vTaskDelay(5)` thì alpha xin B (beta đang giữ) và beta xin A (alpha đang giữ). Hai task khởi động cùng lúc nên cả hai đều lọt vào cửa sổ 5 ms đó ngay từ chu kỳ đầu, nên điều kiện "đua đúng thời điểm" gần như chắc chắn xảy ra.

2. Khi treo, `uxTaskGetSystemState` sẽ báo trạng thái gì cho `task_alpha` và
   `task_beta`? Dự đoán tên mutex mỗi task đang chờ (dựa trên đọc code, chưa
   chạy công cụ).  
   Trả lời: Cả `task_alpha` và `task_beta` đều ở `eBlocked` (chờ mutex với `portMAX_DELAY`, nên không phải `eSuspended`), và `ulRunTimeCounter` của hai task đứng yên giữa các lần chụp. `task_alpha` giữ `s_mutex_a` và đang chờ `s_mutex_b`; `task_beta` giữ `s_mutex_b` và đang chờ `s_mutex_a`. Các task còn lại (IDLE, `runtime_stats_task`) vẫn chạy bình thường.

3. Tính reload value cho IWDG với `IWDG_TIMEOUT_MS` của bạn (LSI ~40 kHz).  
   Trả lời: `IWDG_TIMEOUT_MS` = 250 ms. Chọn prescaler /4 thì bộ đếm chạy ở 40000 / 4 = 10 kHz. reload = 250 × (40000 / 4) / 1000 = **2500** (nhỏ hơn giới hạn 12-bit là 4095). Kiểm tra ngược: 2500 / 10 kHz = 250 ms. Do LSI thực tế nằm trong khoảng 30–60 kHz nên timeout thật có thể lệch trong khoảng 167–333 ms.

4. Sau khi sửa lỗi deadlock và thêm supervisor + IWDG, nếu bạn cố tình tái
   tạo lại tình huống deadlock cũ (tạm thời), dự đoán board sẽ phản ứng thế
   nào và sau bao lâu.  
   Trả lời: Deadlock lại xảy ra khoảng 5 ms sau khi boot, hai task không check-in được nên supervisor không đủ bit và không gọi `HAL_IWDG_Refresh`. IWDG hết hạn và reset board sau khoảng 250 ms kể từ lần refresh cuối (do deadlock xảy ra ở chu kỳ đầu nên thực tế là khoảng 250 ms sau khi IWDG được khởi tạo; số đo thật có thể lệch do sai số LSI). Vì code lỗi vẫn còn nên sau mỗi lần reset board lại deadlock rồi lại reset, tạo thành vòng reset lặp mỗi khoảng 250 ms cộng thời gian boot, và trên UART sẽ thấy dòng khởi động lặp đi lặp lại.