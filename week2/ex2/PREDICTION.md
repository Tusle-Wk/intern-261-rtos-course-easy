# Dự đoán — Week 2 / Ex2

> **Commit TRƯỚC khi nạp chương trình.**

`student_id`: 2413858    `WRITER_COUNT` = 3   `MSG_LEN` = 40

1. Ở `UART_GUARD_MODE = 0`: dự đoán lỗi lẫn lộn sẽ xuất hiện sau khoảng bao
   nhiêu dòng log? Vì sao MSG_LEN và WRITER_PERIOD_MS của bạn ảnh hưởng đến
   con số này?

   **Dự đoán: lỗi xuất hiện ngay ở chu kỳ in đầu tiên (trong 1–3 dòng log đầu) và lặp lại ở gần như mọi chu kỳ sau đó (≥ 90% chu kỳ có ít nhất một dòng bị lẫn).**

   - Mỗi byte mất 10 bit / 230400 baud ≈ 43,4 µs, nên một bản tin MSG_LEN = 40 byte mất ≈ 1740 µs. Con số này lớn hơn 1 tick (1 ms), tức là một writer chỉ phát được khoảng 23 byte thì time slicing đã chuyển CPU sang writer khác cùng ưu tiên. Writer kia cũng ghi thẳng vào `USART1->DR`, nên log chu kỳ đầu dự kiến có dạng: ~23 byte đầu của W0 (`W0 #00000 aaaa…`), rồi ~23 byte đầu của W1, rồi ~23 byte đầu của W2, sau đó mới tới phần còn lại (~17 byte, có `\r\n`) của W0, W1, W2. Nếu MSG_LEN ≤ ~23 byte (in xong trong 1 tick) thì lỗi sẽ hiếm hơn nhiều.
   - WRITER_PERIOD_MS = 7 ms: tổng thời gian UART cần cho 3 writer là 3 × 1,74 = 5,2 ms < 7 ms, nên về nguyên tắc nếu các writer lệch pha thì sẽ không đè nhau. Nhưng cả 3 writer được tạo cùng lúc, cùng chu kỳ, và sau mỗi lần lẫn lại kết thúc trong cùng khoảng ~1 tick, nên chúng thức dậy cách nhau chỉ 1–2 tick (< 1,74 ms) và đè nhau lại ở mọi chu kỳ. Vì vậy lỗi tái hiện ổn định chứ không phải tình cờ.

2. Dự đoán thứ tự **độ trễ in** (latency) từ thấp đến cao giữa mutex / critical
   section / gatekeeper. Vì sao?

   **Dự đoán (trung bình): mutex (≈ 1,75 ms) ≲ gatekeeper (≈ 1,78 ms) < critical section (≈ 3,5 ms).**
   **Dự đoán (max): gatekeeper ≈ 1,8 ms < mutex ≈ 4,2 ms < critical section ≈ 5,2 ms.**

   - Thời gian truyền thuần ≈ 1,74 ms là nền của cả ba cách.
   - **Mutex:** ở trạng thái ổn định 3 writer tự lệch pha. `vTaskDelay(7)` tính từ lúc in xong và làm tròn theo tick, nên mỗi writer cách nhau ≈ 2 ms ≥ 1,74 ms và không còn tranh chấp. Chỉ chu kỳ đầu có writer phải chờ (W1 ≈ 2,5 ms, W2 ≈ 4,2 ms), nên trung bình gần như không bị kéo lên, nhưng max thì thấy.
   - **Gatekeeper:** ưu tiên 3 cao hơn writer (1) nên mỗi lần writer bỏ bản tin vào queue, gatekeeper chiếm CPU ngay và in xong rồi mới trả lại. `t_req` chỉ được đóng dấu khi writer thực sự chạy, nên queue không bao giờ chứa quá 1 phần tử và độ trễ ≈ 1,74 ms + vài chục µs (memcpy, copy qua queue hai lần, các lần đổi ngữ cảnh). Vì thế cao hơn mutex một chút, max gần như bằng trung bình.
   - **Critical section:** cao nhất, và đây là điểm khác với trực giác. Mọi đoạn che ngắt dài 1,74 ms đều chứa ít nhất một mốc tick, nên khi thoát critical section, SysTick đã pending và chạy ngay; time slicing chuyển sang writer kế đang ready **trước khi** writer vừa in kịp gọi `lat_record`. Hơn nữa do mất tick, cả 3 writer thức dậy cùng một tick kernel và lặp lại chuỗi này mỗi chu kỳ: writer đầu in xong lúc 1,74 ms nhưng chỉ ghi số đo sau khi cả ba in xong (≈ 5,2 ms), writer hai ≈ 3,5 ms, writer ba ≈ 1,74 ms. Trung bình ≈ (5,2 + 3,5 + 1,74) / 3 ≈ 3,5 ms, max ≈ 5,2 ms, ở mọi chu kỳ chứ không chỉ chu kỳ đầu.

3. Với critical section, task nào trong hệ thống có nguy cơ bị ảnh hưởng xấu
   nhất nếu đoạn in quá dài? (gợi ý: nghĩ đến ngắt, không chỉ task)

   **`probe_task` (ưu tiên 4, chu kỳ 1 ms) và chính SysTick của kernel, cùng mọi ngắt có mức ưu tiên ≤ `configMAX_SYSCALL_INTERRUPT_PRIORITY`.**

   - Trong 1,74 ms ở trong critical section, các ngắt này bị che hoàn toàn. Cờ pending của SysTick chỉ giữ được 1 lần, nên nếu đoạn che chứa 2 mốc tick thì mất 1 tick. Dự đoán khoảng 2 tick mất mỗi vòng 3 bản tin (≈ 0,7 tick mỗi bản tin), tức đồng hồ kernel chạy chậm khoảng 17% so với thời gian thật: `g_tick_lag_ms` tăng đều theo thời gian và mọi `vTaskDelay` bị dãn.
   - `probe_task` bị trễ: `g_probe_max_us` dự đoán ≈ 1,75–1,8 ms (> `PROBE_LATE_US` = 1500), `g_probe_late` tăng khoảng 1 lần cho mỗi bản tin in (3 lần mỗi vòng).
   - Ngắt ngoại vi khác (UART RX, timer, DMA…) trong hệ thống thật sẽ trễ tới ≈ 1,74 ms và có thể mất dữ liệu.

4. RAM dùng thêm của gatekeeper so với mutex: dự đoán tăng hay giảm, vì sao?

   **Tăng, khoảng 17 lần.** Mutex chỉ là một `Queue_t` ≈ 80–90 byte. Gatekeeper cần:
   - Queue: 80 byte điều khiển + 6 phần tử (`GATEKEEPER_QUEUE_LEN` = 3×2) × 44 byte (40 byte dữ liệu + 4 byte `t_req`) = 264 byte, tổng ≈ 344 byte.
   - Stack gatekeeper: 256 word × 4 = 1024 byte.
   - TCB ≈ 90 byte.

   Tổng ≈ 1,4–1,5 kB (chưa tính phần đệm heap). Critical section: 0 byte thêm.

5. Nếu `WRITER_COUNT` tăng gấp đôi, cách nào trong 3 cách bị ảnh hưởng về độ trễ
   nhiều nhất? Vì sao?

   **Critical section tăng nhiều nhất, mutex tăng vừa, gatekeeper gần như không đổi.**

   | WRITER_COUNT 3 → 6 | Trung bình | Max |
   |---|---|---|
   | Critical section | ≈ 3,5 → ≈ 6,1 ms | ≈ 5,2 → ≈ 10,4 ms |
   | Mutex | ≈ 1,75 → ≈ 4 ms | ≈ 4,2 → ≈ 9,4 ms |
   | Gatekeeper | ≈ 1,78 → ≈ 1,8 ms | ≈ 1,8 ms |

   - **Critical section:** 6 writer thức dậy cùng tick và chạy chuỗi 6 đoạn che ngắt liên tiếp (6 × 1,74 = 10,4 ms). Writer đầu tiên chỉ ghi số đo sau khi cả 6 in xong (≈ 10,4 ms), writer cuối ≈ 1,74 ms, trung bình ≈ 6,1 ms. Hệ quả kèm theo: ngắt bị che liên tục 10,4 ms trong mỗi vòng ≈ 17 ms (≈ 60% thời gian, các đoạn nối liền nhau không có khe hở), mất khoảng 4 tick mỗi vòng (kernel chậm ≈ 23%), `probe_task` bị trễ ở mọi đoạn.
   - **Mutex:** 6 × 1,74 = 10,4 ms > chu kỳ 7 + 1,74 ms nên UART bị bão hòa. Theo mô hình hàng đợi kín, chu kỳ của mỗi writer bị UART quyết định (6 × 1,74 ≈ 10,4 ms), còn thời gian nghỉ thực sau `vTaskDelay(7)` chỉ ≈ 6,5 ms (vì wake-up làm tròn theo tick), nên độ trễ ≈ 10,4 − 6,5 ≈ 4 ms. Max ≈ 9,4 ms xảy ra ở chu kỳ đầu, khi writer cuối phải chờ 5 bản tin phía trước.
   - **Gatekeeper:** độ trễ đo vẫn ≈ 1,75 ms vì `t_req` chỉ đóng dấu khi writer chạy và gatekeeper (ưu tiên cao) in ngay. Chi phí chuyển sang chỗ khác: gatekeeper chiếm ≈ 60% thời gian CPU (10,4 ms mỗi vòng ≈ 17 ms) để polling UART, nên các task ưu tiên thấp hơn nó (writer, idle) chỉ còn ≈ 40% CPU; probe (ưu tiên 4) không bị ảnh hưởng.