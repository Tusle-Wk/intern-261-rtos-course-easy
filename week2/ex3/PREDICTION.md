# Dự đoán — Week 2 / Ex3

> **Commit TRƯỚC khi nạp chương trình.**

`student_id`: 2413858    `LOW_HOLD_MS` = 12  `MID_WORK_MS` = 30
`HIGH_PERIOD_MS` = 35  `DEADLINE_MS` = 15

1. Cấu hình binary semaphore: thời gian `task_high` bị block dự đoán = 68 ms.
   Phép tính dựa trên đâu (liệt kê các thành phần cộng lại)?

   Lần đầu `task_high` bị chặn (t0 ≈ 35 ms): `task_low` đã chạy được khoảng
   4 ms trong tổng 12 ms giữ tài nguyên, còn lại khoảng 8 ms. Nhưng
   `task_low` chỉ có CPU trong các khoảng nghỉ 5 ms của `task_mid`
   (`vTaskDelay` 5 ms), nên không xong trong một lần. Cộng lại:
   - 1 ms `task_low` chạy tiếp ngay sau t0 (còn khoảng 7 ms)
   - 30 ms `task_mid` chen lần 1 (MID_WORK_MS)
   - 5 ms `task_low` chạy trong khoảng nghỉ của `task_mid` (còn khoảng 2 ms)
   - 30 ms `task_mid` chen lần 2 (MID_WORK_MS)
   - 2 ms `task_low` chạy nốt rồi nhả tài nguyên
   Tổng ≈ 1 + 30 + 5 + 30 + 2 = 68 ms. Sai số ±1–2 ms do độ phân giải tick
   1 ms. Con số này phụ thuộc pha thời gian giữa các task và hằng số nghỉ của
   `task_mid` (5 ms) tôi chọn trong code, nên các chu kỳ sau có thể lệch.
2. `task_high` có đạt `DEADLINE_MS` ở cấu hình binary semaphore không? Vì sao?

   Không đạt. Block dự đoán 68 ms lớn hơn nhiều so với 15 ms. Ngay cả khi chỉ
   có một lần `task_mid` chen vào thì block cũng khoảng 7 + 30 = 37 ms, vẫn
   vượt 15 ms. Nguyên nhân là `task_mid` có ưu tiên cao hơn `task_low` nên
   chiếm CPU, `task_low` không nhả được tài nguyên.
3. Cấu hình mutex: thời gian `task_high` bị block dự đoán = 8 ms. Vì sao
   ngắn hơn (hoặc không) so với câu 1?

   Với mutex, khi `task_high` gọi take và bị chặn, `task_low` được nâng lên
   ưu tiên của `task_high` (priority inheritance). Lúc đó `task_mid` (ưu tiên
   thấp hơn) không preempt được `task_low`. `task_low` chỉ cần chạy nốt phần
   còn lại của lần giữ tài nguyên (khoảng 8 ms ở lần đầu) rồi nhả, ưu tiên
   trở về như cũ. Block bị chặn trên bởi LOW_HOLD_MS = 12 ms và không còn
   cộng thêm MID_WORK_MS. 8 ms < 12 ms < DEADLINE_MS = 15 ms nên đạt deadline.
4. Trên ảnh logic analyzer ở cấu hình binary semaphore, kênh `task_mid` sẽ ở
   trạng thái nào trong lúc `task_high` bị chặn — chạy liên tục, chạy ngắt
   quãng, hay không chạy? Vì sao?

   Chạy ngắt quãng: hai đợt dài khoảng 30 ms, ngăn cách bởi một khoảng nghỉ
   khoảng 5 ms (`vTaskDelay`). Trong khoảng nghỉ đó `task_mid` ở mức thấp và
   `task_low` mới thực sự có CPU. Kênh `task_low` (PA0) đánh dấu "đang giữ
   tài nguyên", không phải "đang chạy", nên nó cao liên tục suốt khoảng này
   (kể cả lúc `task_low` bị preempt) và chỉ xuống khi nhả tài nguyên. Chính hai
   đợt chạy của `task_mid` tạo ra phần kéo dài của inversion.
5. Nếu đổi `task_mid` sang ưu tiên **thấp hơn** `task_low`, inversion còn xảy ra
   không? Vì sao (đây là cách phân biệt priority inversion với việc chỉ đơn
   giản là "task_low chạy lâu").

   Không còn inversion. `task_mid` không preempt được `task_low`, nên
   `task_low` chạy liên tục và nhả tài nguyên sau tối đa LOW_HOLD_MS = 12 ms.
   `task_high` chỉ chờ phần còn lại của lần giữ tài nguyên (≤ 12 ms) cả ở
   binary semaphore lẫn mutex. Đó chỉ là chờ `task_low` chạy xong phần việc
   của nó. Inversion thực sự cần một task ưu tiên giữa chen vào, làm thời gian
   block vượt quá thời gian giữ tài nguyên.