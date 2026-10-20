# Dự đoán — Week 1 / Ex3

> **Commit TRƯỚC khi nạp chương trình.**

`student_id`: 2413858    `PERIOD_MS` = 17    `WORK_JITTER_US` = 1400    `SAMPLE_COUNT` = 800

1. Bảng dự đoán (điền trước khi đo):

| # | Delay | Tick | Chu kỳ tb (ms) | Jitter (us) | Drift tổng sau SAMPLE_COUNT (ms) |
|---|---|---|---|---|---|
| 1 | `vTaskDelay` | 1000 | ~17.7 ms (hoặc ~17.29 ms do lượng tử hóa tick) | ~1000 - 1400 us | ~ +560 ms (lý thuyết) / ~ +230 ms (theo tick) |
| 2 | `xTaskDelayUntil` | 1000 | 17.0 ms | ~0 - 50 us (chỉ phụ thuộc jitter ngắt SysTick) | ~0 ms |
| 3 | `vTaskDelay` | 100 | ~10.7 ms (làm tròn tick 10 ms + tải giả) | ~1400 - 10000 us | ~ -5040 ms so với chu kỳ đích 17 ms (+560 ms so với nền 10 ms) |
| 4 | `xTaskDelayUntil` | 100 | 10.0 ms | ~0 - 50 us | -5600 ms so với chu kỳ đích 17 ms (~0 ms so với nền 10 ms) |

2. Cấu hình nào có drift lớn nhất? Vì sao, viết bằng công thức có `WORK_JITTER_US`.
- Nếu xét drift tích lũy do tải biến thiên (không tính sai số làm tròn tick ở 100 Hz): Cấu hình dùng `vTaskDelay` (cấu hình 1 và 3) có drift lớn nhất vì `vTaskDelay` trì hoãn tương đối kể từ thời điểm kết thúc công việc. Thời gian thực thi tải giả bị cộng dồn vào từng chu kỳ:
  `Drift_tổng = SAMPLE_COUNT * (WORK_JITTER_US / 2) = 800 * (1.4 ms / 2) = +560 ms`.
- Nếu xét tổng độ lệch so với chu kỳ thiết kế `PERIOD_MS = 17 ms`: Cấu hình 3 và 4 (tick 100 Hz) bị lệch lớn nhất (-5600 ms sau 800 chu kỳ) do sai số làm tròn xuống của `pdMS_TO_TICKS(17)` từ 17 ms xuống 10 ms.
3. `PERIOD_MS` = 17 ms . Với tick 100 Hz, một tick = 10 ms, nên chu kỳ thực tế sẽ
   bị làm tròn thành 10 ms. Sai số hệ thống: |(10 - 17) / 17| * 100% = 41.18 %.
4. Jitter ở tick 1000 Hz so với 100 Hz: lớn hơn / nhỏ hơn / bằng? Vì sao?
- Với `vTaskDelay`: Độ phân giải của bộ lập lịch phụ thuộc vào độ dài 1 tick. Ở 1000 Hz, bước nhảy lượng tử hóa thời gian khi `WORK_JITTER_US` vượt ranh giới tick là 1 ms, trong khi ở 100 Hz bước nhảy lên tới 10 ms, nên khi rơi vào biên giao chuyển tick thì jitter ở 100 Hz sẽ lớn hơn gấp 10 lần.
- Với `xTaskDelayUntil`: Vì lệnh đảo chân GPIO nằm ngay đầu vòng lặp trước khi chạy tải giả và `WORK_JITTER_US` (1.4 ms) luôn nhỏ hơn 1 tick của cả hai cấu hình (10 ms ở 100 Hz) và nhỏ hơn `PERIOD_MS` (17 ms), task luôn thức dậy đúng sườn ngắt SysTick nên jitter rất nhỏ ở cả hai tần số.
5. Nếu task này **không** phải ưu tiên cao nhất, đại lượng nào trong bảng bị ảnh hưởng nhiều nhất? 
- Jitter (do giá trị max tăng lên). Khi task thức dậy từ trạng thái Blocked sang Ready nhưng CPU đang bận chạy một task khác có độ ưu tiên cao hơn (preemption), task tuần hoàn phải chờ trong hàng đợi Ready khiến thời điểm toggle GPIO bị trễ bất thường, làm vọt giá trị max dẫn đến tăng jitter.
