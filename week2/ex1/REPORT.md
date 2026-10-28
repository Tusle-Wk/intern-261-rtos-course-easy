# Report — Week 2 / Ex1

`student_id`: 2413858    Commit của PREDICTION.md: 337edc9

Tham số: `SAMPLE_PERIOD_MS`=5, `QUEUE_LENGTH`=6, `FILTER_WINDOW`=16, `UART_BAUD`=230400 (cấu hình bình thường), 9600 (UART chậm).

Các lần chạy: **A, A2** (230400, timeout 0), **B** (9600, `FILTER_SEND_TIMEOUT`=0), **T2** (9600, `FILTER_SEND_TIMEOUT`=`pdMS_TO_TICKS(2)`), **T3** (9600, `FILTER_SEND_TIMEOUT`=`portMAX_DELAY`, `SENSOR_SEND_TIMEOUT`=0), **T4** (9600, cả hai = `portMAX_DELAY`). Mỗi lần: log độ đầy 2000 mẫu x 1 ms (`log_raw_X.bin`, `log_filt_X.bin`) và log terminal khoảng 5 giây (`term_X.txt`). Hai lần **N16** và **N32** (230400 baud, timeout 0, `FILTER_WINDOW` = 16 và 32, dữ liệu cảm biến giả) chỉ dùng cho mục 6 và chỉ lưu `term_N16.txt`, `term_N32.txt`.

## 1. Kích thước queue tối thiểu
| | Dự đoán | Đo được | Khớp? |
|---|---|---|---|
| `queue_raw` | 1 (khuyến nghị 2) | 1 (`raw_peak` = 1, A2); logger chỉ thấy 0 | Khớp |
| `queue_filtered` | 1 (khuyến nghị 2) | 1 (`filt_peak` = 1 và logger, A2) | Khớp |

Ở cấu hình bình thường cả hai queue không bao giờ đầy (max 1 trên `QUEUE_LENGTH`=6, dư 6 lần), bộ đếm `sensor_drops`, `filter_drops`, `filter_seq_gaps`, `uart_lost`, `i2c_errors`, `uart_tx_errors` đều bằng 0, và `term_A.txt` (1030 dòng, seq 0..1029) và `term_A2.txt` (1008 dòng, seq 0..1007) không có lỗ nào trong seq: **không mất mẫu**.

Nếu lệch: `queue_raw` do logger đo được 0 chứ không phải 1, vì `depth_logger_task` có ưu tiên thấp hơn `filter_task`. Mỗi khi sensor gửi một mẫu, filter chạy trước logger và lấy mẫu đó ra, nên logger chỉ thấy 0. Mức 1 chỉ kéo dài vài chục µs nên chỉ bộ đếm `raw_peak` (đọc độ đầy ngay sau `xQueueSend` trong sensor) mới bắt được. Hai nguồn đo cùng cho kết luận N_min = 1.

## 2. Đồ thị độ đầy queue theo thời gian
Cấu hình bình thường: `![](queue_fill_normal.png)`
Cấu hình UART chậm (9600 baud): `![](queue_fill_slow_uart.png)`

**Cấu hình bình thường (230400 baud).** `queue_raw` phẳng ở 0. `queue_filtered` là chuỗi xung lên 1 rồi về 0: 400 điểm > 0 trong 2000 ms, các xung cách nhau đều 5 ms (399 khoảng cách đều bằng 5 ms), đúng bằng 2000 / `SAMPLE_PERIOD_MS`. Mỗi xung rộng khoảng 1 mẫu logger (~1 ms): đó là thời gian mẫu nằm chờ trong queue cho tới khi `uart_task` (ưu tiên thấp nhất) được chạy và lấy ra. Ở thang 2000 ms các xung dày đến mức dính thành một khối đặc. Đồ thị A và A2 giống hệt nhau.

**Cấu hình UART chậm (9600 baud).** `queue_raw` vẫn bằng 0. `queue_filtered` tăng theo bậc thang từ 0 ms: mỗi 5 ms có thêm một mẫu vào, trong khi UART chỉ lấy ra một mẫu mỗi ~21 ms, nên queue chạm 6 (đầy) lúc 35 ms. Sau đó queue giữ ở 6 (trung bình 5,84 trên cả log), thỉnh thoảng gạch xuống 5: đó là lúc `uart_task` lấy một mẫu ra và `filter_task` lấp lại chỗ trống ở mẫu kế tiếp. Queue luôn đầy nên mỗi mẫu được nhận phải chờ khoảng 6 x 21 ms ≈ 125 ms trước khi đến UART.

## 3. Khi nào queue đầy
Cấu hình B (9600 baud, `FILTER_SEND_TIMEOUT` = 0):

- `queue_filtered` bắt đầu > 0 tại **0 ms** (mẫu seq 0 được gửi ngay khi sensor bắt đầu chạy) và **đầy (= 6) lần đầu tại 35 ms** (độ phân giải logger 1 ms).
- **Mẫu đầu tiên bị mất có seq = 8.** Các seq 0..7 đến UART đầy đủ, rồi nhảy thẳng sang seq 9. Giải thích bằng sức chứa: `uart_task` lấy seq 0 ngay khi có (dòng 19 byte x 1,042 ms ≈ 19,8 ms), lấy tiếp seq 1 lúc ~20 ms, và còn đang gửi seq 1 lúc seq 8 đến (40 ms). Lúc đó queue chứa seq 2..7 (đủ 6), nên seq 8 bị bỏ. Tức 2 mẫu đã được UART lấy + 6 chỗ trong queue = 8 mẫu đầu được giữ.
- Số mẫu mất đếm từ `term_B.txt`: **731 mẫu mất trên 957** (seq cuối = 956 ở dòng hợp lệ cuối, 226 dòng nhận được), tức **76,4% bị mất**, **23,6% đến UART**. Hai dòng liên tiếp cách nhau 4 hoặc 5 seq (138 lần cách 4, 79 lần cách 5), tức một dòng mỗi ~21 ms, đúng với dòng 21 byte x 1,042 ms ≈ 21,9 ms.
- Bộ đếm lúc halt: `filter_drops` = 756, `uart_lost` = 735, `sensor_drops` = 0, `filter_seq_gaps` = 0. Mẫu mất hoàn toàn ở `filter_task` (gửi vào `queue_filtered` với timeout 0). Chênh lệch `filter_drops` - `uart_lost` = 21 là các mẫu đã bị bỏ nhưng nằm giữa 6 mẫu còn trong queue chưa tới UART lúc halt (mỗi mẫu cách nhau ~3 mẫu bị bỏ).
- `term_B.txt` dài hơn log RAM: log độ đầy chỉ phủ 2 s đầu, còn số mẫu mất đếm trong ~5 s.

## 4. Hành vi 3 kiểu timeout
| Timeout | Kết quả quan sát khi queue đầy |
|---|---|
| 0 | **Lần B.** `xQueueSend` trả về `errQUEUE_FULL` ngay, mẫu bị bỏ, task không bị chặn nên nhịp lấy mẫu giữ nguyên. Mất ở `filter_task`: `filter_drops` = 756, `sensor_drops` = 0, `queue_raw` max = 0. Seq mất đầu tiên = 8, mất 76,4% (731/957). Lỗ xuất hiện trong seq ở terminal. |
| giá trị hữu hạn | **Lần T2 (filter chờ tối đa 2 ms).** Filter ở trạng thái Blocked tối đa 2 ms; nếu vẫn không có chỗ thì bỏ mẫu như timeout 0. Kết quả tổng gần như B: `filter_drops` = 788, `uart_lost` = 766, mất 762/997 = 76,4%, `sensor_drops` = 0, `queue_raw` max = 0 (2 ms < 5 ms nên không kịp tích mẫu trong queue_raw). Thông lượng bị UART quyết định chứ không phải timeout. Khác biệt quan sát được: seq mất đầu tiên dịch từ 8 sang **9** (seq 8 đến lúc UART vừa lấy xong một mẫu nên được chờ và nhận), và đồ thị ít gạch xuống 5 hơn B vì filter đang chờ sẽ lấp chỗ trống ngay. |
| `portMAX_DELAY` | **Lần T3 (chỉ filter dùng `portMAX_DELAY`, sensor timeout 0).** Filter Blocked khi `queue_filtered` đầy nên không bỏ mẫu (`filter_drops` = 0), nhưng nút nghẽn dồn ngược: `queue_raw` bắt đầu tăng lúc ~50 ms và đầy lúc 80 ms, rồi sensor bỏ mẫu (`sensor_drops` = 747, `filter_seq_gaps` = 726, `uart_lost` = 702). Mất mẫu **chuyển chỗ chứ không biến mất**, tổng vẫn ~76% (699/924). Seq mất đầu tiên = **18** = 5 mẫu UART đã lấy + 6 (queue_filtered) + 1 (filter đang giữ) + 6 (queue_raw).<br><br>**Lần T4 (cả filter và sensor dùng `portMAX_DELAY`).** Không mất mẫu nào: `sensor_drops`, `filter_drops`, `filter_seq_gaps`, `uart_lost` đều bằng 0, `term_T4.txt` có seq 0..282 liên tục (283 dòng), `raw_peak` = 6 và `filt_peak` = 6. Cả hai queue đầy (`queue_filtered` lúc 35 ms, `queue_raw` lúc 85 ms) rồi đứng yên ở 6, không còn gạch xuống 5. Sensor bị Blocked trong `xQueueSend` và **chu kỳ lấy mẫu bị phá**, có bằng chứng trực tiếp từ mảng `send_tick` (200 mẫu): 18 khoảng đầu cách nhau đúng **5 ms** (tick 0 đến 90, seq 0..18), từ seq 19 trở đi khoảng cách nhảy lên **17 đến 21 ms** (181 khoảng, trung bình **19,7 ms**). Nhịp lấy mẫu từ 200 Hz tụt xuống khoảng 51 Hz, bằng tốc độ UART gửi một dòng 17 đến 19 byte ở 9600 baud (17,7 đến 19,8 ms). Mẫu đầu tiên bị chặn là seq 19 = 6 mẫu UART đã lấy + 6 (queue_filtered) + 1 (filter đang giữ) + 6 (queue_raw), hơn T3 một mẫu vì dòng ngắn hơn nên UART lấy ra được 6 mẫu thay vì 5 trong 90 ms đầu. Seq vẫn liên tục (không có lỗ) nhưng thời gian giữa các mẫu không còn đều. |

## 5. Lệch so với dự đoán
1. **`queue_raw` đo được 0 thay vì "thỉnh thoảng nhảy lên 1".** Nguyên nhân: logger ưu tiên thấp hơn `filter_task` nên chỉ chạy sau khi filter đã lấy hết mẫu. Đây là hạn chế của phương pháp đo, không phải sai ở dự đoán: bộ đếm `raw_peak` = 1 (A2) xác nhận mức 1 có thật và N_min = 1 đúng như dự đoán.
2. **Độ dài dòng UART ngắn hơn dự đoán.** PREDICTION tính dòng 29 đến 33 byte (seq tối đa 10 chữ số). Thực tế seq nhỏ hơn 1000 nên dòng chỉ 17 đến 21 byte (độ dài còn phụ thuộc giá trị gia tốc: lần B dòng 19 đến 21 byte, lần T4 dòng 17 đến 19 byte do giá trị cảm biến nhỏ hơn). Hệ quả: t_uart ở 230400 baud ≈ 0,9 ms (xung ở đồ thị A rộng ~1 ms) thay vì 1,43 ms; ở 9600 baud ~21 ms (B) hoặc ~19,7 ms (T4) thay vì ~30 ms. Dự đoán "21 đến 34 ms" ở 9600 vẫn đúng, nhưng đo được nằm ở đầu thấp.
3. **Thời điểm đầy 35 ms** nằm ở cận trên của khoảng dự đoán 30 đến 35 ms, vì `uart_task` lấy được 2 mẫu ra (lúc ~0 và ~20 ms) trước khi queue đầy, chậm hơn ước lượng đơn giản 6 x 5 = 30 ms.
4. **Tỉ lệ mất 76%** nằm ở cận dưới của khoảng dự đoán 75 đến 85%, cũng vì dòng ngắn hơn dự đoán nên UART lấy ra nhiều hơn.
5. **Seq mất đầu tiên = 8** khớp dự đoán "xấp xỉ 8 (có thể 9)"; T2 cho 9, cũng nằm trong khoảng dự đoán. T3 cho 18 và T4 bị chặn đầu tiên ở seq 19 là các trường hợp PREDICTION không tính cụ thể; cả hai giải thích được bằng sức chứa của pipeline (mục 4).
6. **Chu kỳ lấy mẫu ở T4 (`portMAX_DELAY`).** PREDICTION dự đoán sensor Blocked, seq liên tục và mẫu không còn cách đều, kèm ý "chạy bù liên tiếp" sau khi thức. Đo được: seq liên tục và chu kỳ bị phá đúng như dự đoán, nhưng không thấy chạy bù thành cụm: 18 khoảng đầu cách nhau 5 ms, sau đó đều đặn 17 đến 21 ms (trung bình 19,7 ms). Lý do: mỗi lần sensor thức lại bị chặn tiếp ở `xQueueSend` vì queue vẫn đầy, nên nhịp lấy mẫu bằng đúng nhịp UART lấy một mẫu ra.

## 6. Câu hỏi đánh đổi
Với `SAMPLE_PERIOD_MS` = 5 ms của tôi, tăng `FILTER_WINDOW` từ 16 lên 32.

**Dự đoán bằng số.** Bộ lọc dùng running sum O(1) nên thời gian tính không đổi theo kích thước cửa sổ; độ trễ tăng thêm nằm ở độ trễ nhóm của trung bình trượt, bằng (N - 1) / 2 mẫu:
- N = 16: (16 - 1) / 2 = 7,5 mẫu = 7,5 x 5 = 37,5 ms.
- N = 32: (32 - 1) / 2 = 15,5 mẫu = 15,5 x 5 = 77,5 ms.
- Tăng thêm: 8 mẫu = **40 ms**.

Độ trễ do hàng đợi ở 230400 baud không đổi (queue chỉ giữ ~1 mẫu), nên độ trễ cảm biến đến UART tăng thêm cũng khoảng 40 ms.

**Kiểm chứng bằng đo.** Cấu hình: 230400 baud, `FILTER_SEND_TIMEOUT` = `SENSOR_SEND_TIMEOUT` = 0, `FILTER_WINDOW` = 16 rồi 32. Trong `sensor_task.c` tôi tạm thay dữ liệu cảm biến bằng bước nhảy giả: `ax` = 0, rồi nhảy lên 10000 tại seq 100, xuống 0 tại seq 200, và lặp lại mỗi 100 mẫu. Đo số mẫu từ seq 100 đến khi `ax` ở đầu ra bộ lọc đạt 50% bước nhảy (5000), đọc từ `term_N16.txt` và `term_N32.txt` bằng `latency.py`. Cả hai file không mất mẫu nào.

| | N = 16 | N = 32 |
|---|---|---|
| Mỗi mẫu sau bước nhảy, đầu ra tăng thêm | 625 (= 10000 / 16) | ~312 (= 10000 / 32) |
| Seq đạt 5000 (50%) | 107 | 115 |
| **Độ trễ đến 50% bước nhảy** | **7 mẫu = 35 ms** | **15 mẫu = 75 ms** |
| Seq đạt đủ 10000 | 115 (sau 16 mẫu) | 131 (sau 32 mẫu) |
| Mẫu mất | 0 | 0 |

**Kết luận:** độ trễ tăng thêm 15 - 7 = 8 mẫu, tức 8 x 5 = **40 ms**, đúng bằng dự đoán +40 ms. Hai số đo (7 và 15 mẫu) thấp hơn độ trễ nhóm lý thuyết (7,5 và 15,5) đúng 0,5 mẫu (2,5 ms), vì mốc 50% chỉ có thể rơi vào một mẫu rời rạc (mẫu thứ N/2 sau bước nhảy); hiệu số giữa hai lần vẫn là đúng 8 mẫu. Độ trễ này đo bằng số mẫu (seq) tại đầu ra bộ lọc rồi nhân với chu kỳ 5 ms (chu kỳ đã được kiểm ở lần A: các xung cách nhau đều 5 ms), nên không gồm thời gian hàng đợi và gửi UART (cỡ 1 ms ở 230400 baud, không phụ thuộc N).

Đánh đổi: cửa sổ lớn hơn cần nhiều mẫu hơn để phản ánh một thay đổi (chậm hơn 40 ms) nhưng làm mượt nhiễu mạnh hơn (với nhiễu trắng, độ lệch chuẩn giảm theo căn N, tức khoảng 1,4 lần khi gấp đôi cửa sổ), còn chi phí CPU không đổi vì running sum là O(1).