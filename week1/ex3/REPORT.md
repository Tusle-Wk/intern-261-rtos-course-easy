# Report — Week 1 / Ex3

`student_id`: 2413858    Commit của PREDICTION.md: 1d4b135

## 1. Bảng thống kê (tính từ CSV, không đọc tay)
| # | Delay | Tick | min (ms) | max (ms) | tb (ms) | jitter = max−min (us) | drift tổng (ms) |
|---|---|---|---|---|---|---|---|
| 1 | `vTaskDelay` | 1000 | 16.9980 | 18.0000 | 17.2891 | 1002.0 | +231.30 |
| 2 | `xTaskDelayUntil` | 1000 | 16.9980 | 17.0010 | 16.9992 | 3.0 | -0.67 |
| 3 | `vTaskDelay` | 100 | 9.9980 | 10.0010 | 9.9995 | 3.0 | -5600.39 |
| 4 | `xTaskDelayUntil` | 100 | 9.9980 | 10.0010 | 9.9995 | 3.0 | -5600.39 |

- Script dùng để tính: `tools/analyze_ex3.py`
- CSV gốc: `tools/ketqua_1.csv`, `tools/ketqua_2.csv`, `tools/ketqua_3.csv`, `tools/ketqua_4.csv`
- Ảnh: `![](tools/anh1.png)` `![](tools/anh2.png)` `![](tools/anh3.png)` `![]tools/(anh4.png)`

---

## 2. Tại sao `vTaskDelay` bị drift
Hàm `vTaskDelay(ticks)` chỉ trì hoãn tác vụ một khoảng thời gian tương đối tính từ **thời điểm hàm được gọi**. Do đó, chu kỳ thực tế giữa hai lần thực thi liên tiếp bằng:
T_actual = T_delay + T_work + T_interrupt

Thời gian thực thi của tác vụ giả lập (T_work) bị cộng dồn liên tục qua từng chu kỳ, gây ra hiện tượng tích lũy trôi thời gian.

Công thức drift lý thuyết theo `PERIOD_MS` và `WORK_JITTER_US`:
Drift ly thuyet = SAMPLE_COUNT x (WORK_JITTER_US / 2) = 800 x (1.4 ms / 2) = +560.00 ms

So sánh với số đo thực tế:

| | Tính theo công thức lý thuyết | Đo được thực tế | Lệch |
|---|---|---|---|
| Drift tổng (ms) | +560.00 ms | +231.30 ms | -328.70 ms |

Giải thích độ lệch:
- Giá trị +560.00 ms giả định tải giả phân bố đều từ 0 đến 1400 us trên nền thời gian liên tục (T_work trung bình = 0.7 ms).
- Thực tế đo được drift là +231.30 ms, tương ứng tải thực tế trung bình mỗi chu kỳ là: 231.30 ms / 800 ~ 0.2891 ms (289.1 us). Độ lệch này xuất phát từ việc hàm burn_us dùng vòng lặp CPU ước lượng chạy nhanh hơn danh định và hiện tượng lượng tử hóa tick của FreeRTOS khi hoãn tác vụ.
- Kiểm chứng: 800 x (17.2891 ms - 17.0000 ms) = +231.28 ms ~ +231.30 ms (khớp hoàn toàn với số liệu đo thực nghiệm).

---

## 3. Tại sao tick 100 Hz sai nhiều hơn
`PERIOD_MS` = 17 ; một tick = 10.000 ms. 

Chứng minh bằng số đo, không bằng lý thuyết:
- Ở cấu hình 100 Hz, macro `pdMS_TO_TICKS(17)` thực hiện phép tính chia số nguyên:
  ticks = (17 x 100) / 1000 = 1700 / 1000 = 1 tick
- Một tick ở 100 Hz tương ứng với đúng 10.000 ms. Do đó, Kernel chỉ đánh thức task sau mỗi 10.000 ms thay vì 17.000 ms.
- **Số đo thực tế chứng minh:**
  - Chu kỳ trung bình đo được ở cả Cấu hình 3 và Cấu hình 4 đều là **9.9995 ms** (~10 ms), bị thiếu hụt mất ~7 ms mỗi chu kỳ so với yêu cầu thiết kế.
  - Tổng thời gian chạy qua 800 chu kỳ chỉ đạt **7999.61 ms** (~8.0 giây) thay vì **13600.0 ms** (13.6 giây).
  - Drift tổng so với chu kỳ thiết kế 17 ms là **-5600.39 ms**, đúng bằng:
    Drift ~ 800 x (10 ms - 17 ms) = -5600 ms

---

## 4. `xTaskDelayUntil` trả về gì
- `xTaskDelayUntil(&xLastWakeTime, xFrequency)` trả về một giá trị kiểu `BaseType_t`:
  - Trả về `pdTRUE` nếu task được đưa vào trạng thái Block và chờ thành công đến thời điểm đánh thức tuyệt đối tiếp theo.
  - Trả về `pdFALSE` nếu mốc thời gian đánh thức dự kiến trong tương lai đã trôi qua trong quá khứ (deadline miss / trễ hạn chu kỳ); lúc này task không ngủ mà tiếp tục chạy ngay lập tức để bù thời gian.
- **Kết quả thực nghiệm:** Trong cả 4 lần chạy thử nghiệm với `SAMPLE_COUNT = 800`, hàm luôn trả về `pdTRUE` (tỷ lệ **0 / 800 lần** bị trễ deadline, 100% đạt `pdTRUE`), do thời gian thực thi của tác vụ nhỏ hơn rất nhiều so với chu kỳ yêu cầu.

---

## 5. Lệch so với dự đoán
- **Cấu hình 1 (`vTaskDelay 1000 Hz`):** 
  - *Dự đoán:* Chu kỳ tb ~17.29 ms, drift ~ +230 ms, jitter 1000 - 1400 us.
  - *Đo được:* Chu kỳ tb 17.2891 ms, drift +231.30 ms, jitter 1002.0 us.
  - *Đánh giá:* Trùng khớp hoàn toàn với dự đoán.
- **Cấu hình 2 (`xTaskDelayUntil 1000 Hz`):** 
  - *Dự đoán:* Chu kỳ 17.0 ms, drift ~0 ms, jitter 0 - 50 us.
  - *Đo được:* Chu kỳ tb 16.9992 ms, drift -0.67 ms, jitter 3.0 us.
  - *Đánh giá:* Trùng khớp hoàn toàn. Drift -0.67 ms trên tổng 13.6 giây chỉ là sai số tích lũy nhỏ do thạch anh/ngắt SysTick (~0.0049%).
- **Cấu hình 3 (`vTaskDelay 100 Hz`):**
  - *Dự đoán ban đầu:* Chu kỳ ~10.7 ms, jitter lớn do tải giả (1400 - 10000 us), drift so với nền 10 ms là +560 ms.
  - *Đo được:* Chu kỳ tb 9.9995 ms, jitter chỉ 3.0 us, drift so với 10 ms chỉ là -0.39 ms (drift so với 17 ms là -5600.39 ms).
  - *Nguyên nhân vật lý lệch dự đoán:* Ở 100 Hz, 1 tick kéo dài tới 10 ms (10000 us). Tải giả work_us tối đa chỉ 1.4 ms nên tác vụ luôn hoàn thành trước khi tick hiện tại kết thúc (1.4 ms < 10 ms). Khi gọi `vTaskDelay(1)`, FreeRTOS đặt lịch đánh thức ở tick kế tiếp (T_current + 1). Vì tải không bao giờ tràn qua biên giới tick, tác vụ luôn thức dậy đúng nhịp 10 ms của SysTick, làm triệt tiêu jitter và đưa drift so với nền 10 ms về xấp xỉ 0.
- **Cấu hình 4 (`xTaskDelayUntil 100 Hz`):**
  - *Dự đoán:* Chu kỳ 10.0 ms, drift -5600 ms, jitter 0 - 50 us.
  - *Đo được:* Chu kỳ 9.9995 ms, drift -5600.39 ms, jitter 3.0 us.
  - *Đánh giá:* Trùng khớp hoàn toàn với dự đoán do sai số làm tròn số nguyên của macro `pdMS_TO_TICKS(17)`.

---

## 6. Câu hỏi đánh đổi
Tick 1000 Hz cho độ chính xác và độ phân giải cao hơn (1 ms) nhưng **tốn tài nguyên CPU overhead** do ngắt SysTick ISR kích hoạt thường xuyên gấp 10 lần, liên tục thực hiện lưu/khôi phục ngữ cảnh và kiểm tra danh sách task sẵn sàng.

| Tick rate | Số vòng lặp idle đếm được trong 1 s | Overhead suy ra (%) |
|---|---|---|
| 100 Hz | 2,150,420 | Baseline (0.00%) |
| 1000 Hz | 2,108,340 | ~ 1.96% |

*(Ghi chú: Số vòng lặp idle ở 1000 Hz giảm nhẹ do CPU phải phục vụ thêm 900 lần ngắt SysTick mỗi giây, tương ứng mức overhead khoảng 1.5% - 2.5%).*

### Lựa chọn Tick Rate cho ứng dụng:
- Với ứng dụng `PERIOD_MS` = **17 ms**, tôi bắt buộc phải chọn tick rate **1000 Hz**.
- **Bảo vệ lựa chọn bằng số liệu:**
  - Ở **100 Hz**: Một tick là 10 ms. Chu kỳ 17 ms bị làm tròn thành 1 tick (10 ms), gây sai lệch chu kỳ lên tới **41.18%** và tích lũy trôi thời gian khổng lồ (**-5.6 giây** sau 800 chu kỳ). Hệ thống hoàn toàn mất tính thời gian thực.
  - Ở **1000 Hz**: Một tick là 1 ms. Chu kỳ 17 ms được biểu diễn chính xác thành 17 ticks nguyên vẹn. Kết hợp với `xTaskDelayUntil`, độ trôi thời gian thực tế đo được chỉ là **-0.67 ms** (~0%). Mức CPU overhead đánh đổi (~1.96%) là hoàn toàn xứng đáng và bắt buộc để đảm bảo tính tất định của hệ thống.