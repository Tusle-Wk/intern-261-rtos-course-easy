# Dự đoán — Week 3 / Ex3

> **Commit TRƯỚC khi nạp chương trình.**

`student_id`: 2413858

1. Thời gian truyền 1 byte ở `RX_BAUD` của bạn là bao nhiêu us? (8N1, tính cả
   start/stop bit). Trong thời gian đó `parser_task` có kịp xử lý 1 byte từ
   stream buffer không, nếu nó là task ưu tiên cao nhất hệ thống?
   **Trả lời:** Mỗi byte 8N1 gồm 10 bit (1 start + 8 data + 1 stop), `RX_BAUD` = 115200 nên 10 / 115200 = 86,8 us/byte (~11 520 byte/s). Có kịp: phần xử lý một byte gồm ISR (HAL + `xStreamBufferSendFromISR` + arm lại) và `parser_task` (`xStreamBufferReceive` + ghép byte), ước lượng cỡ 10 us (chưa đo), nhỏ hơn 86,8 us nhiều lần. Parser ưu tiên cao nhất (4) và ISR ngắt được mọi task nên không bị chặn. Note: "kịp" chỉ đúng khi tính theo byte; mỗi lệnh parser còn phải `printf` một dòng log (~2,5 ms), đó mới là chỗ parser có thể không kịp khi gửi dồn (cụ thể ở câu 3 và 4).
2. `STREAM_BUFFER_BYTES` của bạn chứa tối đa bao nhiêu lệnh dài `MAX_CMD_LEN`
   byte trước khi đầy?
   **Trả lời:** 192 / 24 = 8 lệnh (lệnh dài nhất là 23 ký tự + `\r` = 24 byte). Nếu parser đứng yên không lấy byte nào thì buffer đầy sau 192 x 86,8 us ≈ 16,7 ms.
3. Nếu `xStreamBufferSendFromISR` trả về 0 (stream buffer đầy) trong
   `uart_rx_isr_handler`, byte đó sẽ bị mất. Theo bạn, trường hợp nào thực tế
   khiến điều này xảy ra (gửi quá nhanh / `parser_task` ưu tiên quá thấp /
   cái khác)?
   **Trả lời:** Cái khác: `parser_task` bị trễ vì `printf` chặn khi gửi dồn liên tục. Mỗi dòng log ~28 ký tự ≈ 2,5 ms, trong khi một lệnh 7–9 byte chỉ mất 0,6–0,8 ms để đến, nên backlog tăng dần và buffer đầy sau khoảng 22–23 ms (~35–40 lệnh). Không phải do "gửi quá nhanh" đơn thuần (11,5 KB/s thì parser dư sức nếu không bị chặn) và không phải do ưu tiên thấp (parser = 4, cao nhất). Nguyên nhân phụ: ISR bị trễ quá 86,8 us thì USART overrun, cũng mất byte nhưng không đi qua `SendFromISR`. Với byte bị mất, ISR không thể chờ hay thử lại nên chỉ đếm vào `uart_rx_dropped`; mất một byte còn có thể biến lệnh thành lệnh hợp lệ khác (vd `P10` thành `P1`), nên phải dựa vào số thứ tự `#seq` để phát hiện.
4. Dự đoán: gửi lệnh bằng gõ tay bình thường có mất byte không? Gửi bằng
   script gửi liên tục không nghỉ thì sao?
   **Trả lời:** Gõ tay: không mất, vì chỉ vài ký tự mỗi giây, các lệnh cách nhau hàng trăm ms, buffer gần như luôn rỗng và parser in log xong trước lệnh kế. Gửi liên tục không nghỉ: có mất, bắt đầu sau khoảng 35–40 lệnh đầu rồi mất tiếp (mô hình đơn giản cho khoảng 70% số byte nếu PC gửi liền không khoảng nghỉ; thực tế adapter USB-UART có thể chèn khoảng nghỉ nên mất ít hơn). Nếu parser không in log thì dự đoán là không mất.