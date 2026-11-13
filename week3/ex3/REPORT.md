# Report — Week 3 / Ex3

`student_id`: 2413858    Commit của PREDICTION.md: 4a1ac15

## 1. Thời gian byte & độ sâu stream buffer
Phép tính thời gian 1 byte ở `RX_BAUD`, và số lệnh tối đa chứa được trong
`STREAM_BUFFER_BYTES`.
**Trả lời:** 8N1 là 10 bit mỗi byte (1 start + 8 data + 1 stop), `RX_BAUD` = 115200 nên 10 / 115200 = 86,8 us/byte. `STREAM_BUFFER_BYTES` = 192, `MAX_CMD_LEN` = 24 (23 ký tự + `\r`) nên chứa tối đa 192 / 24 = 8 lệnh dài nhất.

## 2. Demo 3 loại lệnh
Bằng chứng (log/ảnh) `demo_task` phản ứng đúng với `P`, `S`, `T` — mỗi loại ít
nhất 1 ví dụ có số liệu trước/sau.
**Trả lời:**
- `P50`: log `[P] period 200 -> 50 ms`. PA1 đổi mức mỗi ≈200 ms trước lệnh, mỗi ≈50 ms sau lệnh. Ảnh: `P_serialdebug.png`, `P_pulseview.png`
- `S3` rồi `S1`: log `[S] prio 2 -> 3` và `[S] prio 3 -> 1`. Chu kỳ PA1 không đổi (≈200 ms mỗi lần đổi mức). Ảnh: `S_serialdebug.png`, `S_pulseview.png`
- `T0` rồi `T1`: log `[T] suspend` và `[T] resume`. PA1 đổi mức mỗi ≈200 ms trước `T0`, đứng yên từ ≈2,0 s đến ≈5,56 s (≈3,5 s) sau `T0`, đổi mức lại ngay lúc `T1`. Ảnh: `T_serialdebug.png`, `T_pulseview.png`

## 3. Test gửi nhanh
| | Số lệnh gửi | Số lệnh nhận đúng | Mất/lỗi |
|---|---|---|---|
| Gõ tay bình thường | 20 | 20 | 0 |
| Gửi liên tục (script) | 500 | 38–39 | 461–462 |

Nếu có mất byte: giải thích bằng cơ chế (stream buffer đầy, parser bị trễ,
hay nguyên nhân khác) — không chỉ nói "bị mất".
**Trả lời:** Gõ tay thì các lệnh cách nhau hàng trăm ms nên buffer gần như luôn rỗng, không mất. Gửi liên tục thì mất do stream buffer đầy vì parser bị trễ: một lệnh (~8 byte) đến mỗi ≈0,69 ms, nhưng mỗi lệnh parser phải `printf` một dòng log ≈2,5 ms (blocking), nên backlog tăng dần và buffer 192 B đầy sau ≈22–25 ms (lệnh đầu tiên mất ở khoảng #38). Khi đầy, `xStreamBufferSendFromISR` trả về 0 và byte bị bỏ (đếm trong `uart_rx_dropped`).
Ảnh: `P200_serialdebug.png` (gõ tay, `uart_rx_dropped` = 0), `burst_debug.png` (script, `uart_rx_dropped` = 2408).

## 4. Vì sao stream buffer, không phải queue
Giải thích bằng lời của bạn.
**Trả lời:** UART đưa vào từng byte một và ISR không biết một lệnh dài bao nhiêu hay kết thúc ở đâu, nên dữ liệu là một dòng byte không có ranh giới. Stream buffer lưu thẳng các byte liên tiếp, không tốn thêm chỗ cho mỗi phần tử, còn parser đọc từng byte và tự tìm ranh giới lệnh bằng ký tự `\r`. Queue chỉ chứa các phần tử kích thước cố định, nên muốn dùng thì mỗi byte phải thành một phần tử (tốn bộ nhớ và thời gian sao chép) hoặc ISR phải tự ghép lệnh. Ngoài ra chỉ có một nơi ghi (ISR) và một nơi đọc (`parser_task`), đúng với điều kiện của stream buffer.

## 5. Lệch so với dự đoán
Mỗi mục lệch + nguyên nhân.
**Trả lời:** Dự đoán mất khoảng 70% số byte khi gửi liên tục, nhưng kết quả đo là chỉ 38–39 trong 500 lệnh nhận đúng (khoảng 92% lệnh hỏng), nhiều hơn tỉ lệ byte mất. Nguyên nhân: byte bị bỏ thành từng đoạn liền nhau mỗi lần parser đang `printf`, nên một lệnh bị cắt mất phần đầu hoặc phần cuối, hoặc dính vào lệnh kế tiếp, làm hỏng cả lệnh chứ không chỉ mất vài byte.

## 6. Câu hỏi đánh đổi
Với `RX_BAUD` của bạn, nếu tăng gấp đôi `STREAM_BUFFER_BYTES`, tốc độ gửi tối
đa không mất byte tăng thêm bao nhiêu (đo thật, không suy luận)?
**Trả lời:** Tăng thêm khoảng +5,5%. Đo bằng 500 lệnh mỗi lần thử, tìm gap nhỏ nhất mà cả 3 lần chạy đều nhận đủ 500/500:
- 192 B: gap ≈ 2,84 ms (thử gap 2,8 ms thì mất 9 lệnh), tức ≈ 352 lệnh/s (≈ 2736 B/s).
- 384 B: gap ≈ 2,69 ms (thử gap 2,65 ms thì mất 10 lệnh), tức ≈ 371 lệnh/s (≈ 2887 B/s).

Gấp đôi buffer chỉ tăng chút ít vì tốc độ xử lý của parser (bị `printf` chặn) không đổi, buffer lớn hơn chỉ chứa thêm được phần chênh.
Log: `log_192.txt`, `log_384.txt`.