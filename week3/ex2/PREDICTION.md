# Dự đoán — Week 3 / Ex2

> **Commit TRƯỚC khi nạp chương trình.**

`student_id`: 2413858

1. Với nút nhấn cơ khí thông thường, nảy phím thường kéo dài bao lâu
   (tra cứu/ước lượng)? So với `DEBOUNCE_MS` của bạn: đủ lớn để lọc hết không?

   **Trả lời:** Nút tact switch thông thường nảy khoảng 1–10 ms (thường chỉ vài
   ms). Nút cũ, mòn hoặc rẻ tiền có thể nảy lâu hơn, cỡ vài chục ms. Đây là
   ước lượng từ kiến thức chung, chưa tra datasheet của đúng nút trên board;
   sẽ đối chiếu với số đo logic analyzer.

   `DEBOUNCE_MS` = 20 ms, lớn hơn nảy điển hình khoảng 2–4 lần, nên dự đoán là
   đủ để lọc hết. Điều kiện quyết định không phải tổng thời gian nảy mà là
   khoảng lặng giữa hai cạnh liên tiếp: `debounce_timer` bị reset ở MỖI cạnh,
   nên callback chỉ chạy khi có khoảng lặng >= 20 ms. Trong chùm nảy vài ms,
   khoảng cách giữa các cạnh nhỏ hơn 20 ms rất nhiều, nên mỗi lần nhấn (và mỗi
   lần thả) cho đúng một sự kiện PRESS (RELEASE). Dự đoán chỉ sai nếu nút có
   một khoảng lặng > 20 ms nằm giữa chùm nảy.

2. Nếu `DEBOUNCE_MS` quá lớn, điều gì xảy ra khi người dùng nhấn nhả rất
   nhanh liên tiếp (double-click)?

   **Trả lời:** Mọi cạnh cách nhau ít hơn `DEBOUNCE_MS` bị coi là cùng một
   chùm nảy. Khi double-click, nếu khoảng nhả giữa hai lần nhấn ngắn hơn
   `DEBOUNCE_MS` thì timer liên tục bị reset, không có khoảng lặng đủ dài ở
   giữa, callback chỉ chạy một lần sau cạnh cuối: hai lần nhấn bị gộp. Nếu lúc
   đó phím đang giữ thì chỉ có một PRESS, nếu đã thả thì chỉ còn một RELEASE
   (mất hẳn PRESS). Ví dụ `DEBOUNCE_MS` = 200 ms, click nhanh (giữ ~60 ms,
   nhả ~80 ms) thì không có PRESS nào ở giữa. Click đơn giữ ngắn hơn 200 ms còn
   có thể mất cả PRESS, vì lúc callback chạy phím đã thả nên chỉ in RELEASE.
   Ngoài ra độ trễ phản hồi của nút cũng tăng bằng `DEBOUNCE_MS`. Với 20 ms
   hiện tại, khoảng nhả của người dùng thường từ vài chục đến vài trăm ms nên
   dự đoán không bị nuốt.

3. Khi callback của `debounce_timer` bị block 50 ms (thử nghiệm bước 4), bạn
   dự đoán `longpress_timer` sẽ bị trễ bao nhiêu? Giải thích bằng cơ chế
   daemon task + queue lệnh, không chỉ đoán số.

   **Trả lời:** Trễ tối đa khoảng 50 ms, và chỉ trễ khi hạn của
   `longpress_timer` rơi vào cửa sổ bị block. Nếu hạn cách lúc bắt đầu block
   x ms (0 <= x < 50) thì độ trễ = 50 − x ms; hạn nằm ngoài cửa sổ thì không
   trễ. Ví dụ với chu kỳ thử 40 ms và block bắt đầu ở 20 ms (x = 20): HELD trễ
   30 ms, in ra ở t = 70 với `held=70` thay vì 40.

   Cơ chế: cả hai timer chạy trên MỘT timer daemon task duy nhất. ISR không
   chạy callback, chỉ gửi lệnh (reset/start/stop) vào timer command queue; daemon
   lấy lệnh ra và gọi callback tuần tự. Mỗi vòng lặp của daemon: xử lý tối đa
   MỘT timer đã quá hạn (gọi callback của nó), rồi lấy TOÀN BỘ lệnh đang nằm
   trong queue ra xử lý, rồi mới sang vòng sau.

   Cần phân biệt ba mốc: t_gửi (tick lúc ISR bỏ lệnh vào queue, được đóng dấu
   vào lệnh), t_xử lý (lúc daemon lấy lệnh ra và áp dụng), và hạn E của timer
   (E = t_gửi + chu kỳ, chỉ tính cho Reset/Start). Lệnh có hiệu lực ở t_xử lý
   nhưng hạn tính từ t_gửi. Ngoài ra "hết hạn" chỉ nghĩa là E đã trôi qua;
   callback chỉ chạy khi daemon duyệt tới timer đó, nên timer quá hạn mà chưa
   được xử lý vẫn nằm trong danh sách timer và vẫn bị các lệnh trong queue tác
   động. Kernel không dùng dấu thời gian để sắp thứ tự sự kiện, chỉ xử lý
   theo thứ tự duyệt của vòng lặp.

   `debounce_callback` chạy khoảng 20 ms sau cạnh cuối. Nếu nó block 50 ms thì
   trong cửa sổ đó daemon không làm gì khác: không gọi callback của timer nào
   đã quá hạn, không lấy lệnh trong queue. ISR vẫn chạy và vẫn gửi lệnh được,
   nhưng lệnh nằm chờ trong queue (nếu queue đầy thì `...FromISR` trả về lỗi
   và lệnh bị mất). Khi callback trả về, daemon xử lý hết lệnh đã dồn trong
   queue trước, rồi mới quay lại gọi callback của timer quá hạn kế tiếp. Hệ
   quả:
   - Không có lệnh nào đụng tới `longpress_timer` trong queue: HELD chỉ trễ,
     gọi ngay khi daemon rảnh (độ trễ như công thức ở trên).
   - Người dùng thả phím sau hạn E của longpress_timer nhưng vẫn trong lúc bị 
     block: lệnh Stop được xử lý trước callback HELD đã quá hạn, nên HELD đó 
     bị MẤT thay vì chỉ trễ. (Thả trước E thì HELD không in ra dù không có 
     block, nên không tính là mất.).
   - Có cạnh xuống mới trong lúc bị block: lệnh Start hẹn lại timer theo hạn
     mới tính từ t_gửi, nên callback ở hạn cũ không chạy nữa; HELD đầu tiên
     chỉ xuất hiện ở hạn mới. Nếu hạn mới đã trôi qua lúc daemon xử lý (block
     rất lâu) thì callback được gọi ngay lúc xử lý lệnh.

   Độ trễ không cộng dồn: hạn tính từ t_gửi (không phải lúc daemon xử lý), và
   auto-reload tính hạn kế tiếp từ hạn gốc, nên các lần HELD sau trở về đúng
   lưới 1000 ms.

   Note lúc test
   : với nhấn giữ bình thường, `debounce_callback`
   chạy tại cạnh cuối + 20 ms còn HELD đầu tiên tại cạnh xuống cuối + 1000 ms,
   cách nhau khoảng 980 ms nên không tự trùng. Để thấy trễ, sẽ tạm ép hai mốc
   gần nhau (ví dụ truyền chu kỳ ngắn hơn, cỡ 40 ms, vào `xTimerCreate` của
   longpress ngay trong `debounce.c`, không sửa `params.h`; hoặc kéo dài thời
   gian block) rồi ghi cả hai log timestamp vào REPORT.md.

4. `LONGPRESS_MS` của bạn là bao nhiêu? Giữ phím đúng ngưỡng này, bạn dự đoán
   callback `longpress_callback` chạy lần đầu ở thời điểm nào tính từ lúc
   nhấn xuống?

   **Trả lời:** `LONGPRESS_MS` = 1000 ms. `xTimerStartFromISR` trên timer đang
   chạy sẽ đếm lại từ đầu, nên mốc tính từ cạnh xuống CUỐI CÙNG của chùm nảy:
   `longpress_callback` chạy lần đầu tại (tick cạnh xuống cuối) + 1000 ms. Tính
   từ lúc chạm đầu tiên, dự đoán khoảng 1001–1010 ms (1000 ms cộng thời gian
   nảy vài ms, cộng sai số tối đa 1 tick). Dòng log `held=` tính từ chính cạnh
   xuống cuối nên dự đoán 1000 ms (±1–2 ms). Các lần HELD tiếp theo cách đều
   1000 ms (khoảng 2000, 3000 ms...).

   Nếu thả đúng ở mốc 1000 ms (tính từ lúc chạm đầu) thì lệnh Stop từ cạnh
   thả đến sớm hơn hạn của timer (vì hạn tính từ cạnh xuống cuối, sau cạnh
   chạm đầu), nên HELD thường KHÔNG in ra. Muốn chắc chắn thấy HELD đầu tiên
   thì phải giữ lâu hơn ngưỡng, ví dụ >= 1100 ms.