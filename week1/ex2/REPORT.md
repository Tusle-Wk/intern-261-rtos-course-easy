# Report — Week 1 / Ex2

`student_id`: 2413858    Commit của PREDICTION.md: 074bbdf

## 1. CPU share đo được
| Cấu hình | A (%) | B (%) | C (%) | Time slice đo được | Ảnh |
|---|---|---|---|---|---|
| B2: preempt=1, ưu tiên khác nhau | 0% | 0% | ~100% | — | anh1.png |
| B3: preempt=0, ưu tiên khác nhau | 0% | 0% | ~100% | — | anh2.png |
| B4: preempt=1, cùng ưu tiên | ~33.3% | ~33.3% | ~33.3% | 1.0 ms | anh3.png / anh4.png / anh5.png |
| B5: sau khi sửa starvation | ~24% | ~30% | ~12% | — | anh6.png |

Cách tính CPU share từ ảnh PulseView (mô tả ngắn):
- Dựa trên độ rộng xung hoạt động của từng task so với tổng thời gian quan sát trên logic analyzer trong cùng một khoảng chu kỳ mẫu. Ở B2 và B3, Task A và B không xuất hiện xung (0%), Task C chiếm toàn bộ thời gian. Ở B4, 3 task chia đều thời gian chạy theo các lát cắt Round-Robin.

## 2. Preemption nhìn thấy ở đâu
Trên ảnh **bước 4** (cùng ưu tiên, round-robin), chỉ ra ít nhất **2 thời điểm** preemption xảy ra. Với mỗi thời điểm: dấu thời gian, task bị cắt, task chiếm CPU, và vì sao (hết time slice, không phải block).

| t (us) | Task bị preempt | Task chiếm CPU | Lý do |
|---|---|---|---|
| +40,600 µs | Task A (D0) | Task B (D1) | Hết time slice (1 ms), ngắt SysTick kích hoạt chuyển ngữ cảnh Round-Robin |
| +41,600 µs | Task B (D1) | Task C (D2) | Hết time slice (1 ms), ngắt SysTick kích hoạt chuyển ngữ cảnh Round-Robin |

Ở **bước 2** (ưu tiên khác nhau, đều busy-loop không block): giải thích vì sao ảnh của bạn cho thấy **0 thời điểm preemption**, dù chạy bao lâu đi nữa — dựa vào điều kiện preemption của FreeRTOS, không phải "vì ảnh chụp chưa đủ dài".
- Giải thích: FreeRTOS luôn chọn task có độ ưu tiên cao nhất trong trạng thái Ready để chạy. Ở bước 2, Task C có độ ưu tiên cao nhất (`TASK_C_PRIO = 4`) chạy vòng lặp vô tận không bao giờ gọi hàm gây Block hay từ bỏ CPU. Do Task C liên tục ở trạng thái Ready và có quyền ưu tiên tuyệt đối so với Task A và B, scheduler không bao giờ phải thực hiện chuyển ngữ cảnh sang task ưu tiên thấp hơn, dẫn đến 0 thời điểm preemption xảy ra đối với A và B.

## 3. Time slice
Time slice đo được ở bước 4: 1.0 ms. `configTICK_RATE_HZ` = 1000.
Sai số: 0%. Giải thích sai số:
- Sai số bằng 0% do tần suất ngắt SysTick được định nghĩa chính xác theo `configTICK_RATE_HZ = 1000` Hz, tương ứng đúng chu kỳ 1 ms mỗi tick của kernel FreeRTOS.

## 4. DWT wrap-around
Tại sao so sánh `DWT->CYCCNT > target` bị sai sau ~59 giây, và cách viết đúng:
- Giải thích: `DWT->CYCCNT` là thanh ghi đếm 32-bit. Với xung nhịp `SystemCoreClock = 72 MHz`, thanh ghi này sẽ tràn (wrap-around từ giá trị lớn nhất về 0) sau khoảng 2^32/(72*10^6) ~ 59.65 giây. Phép so sánh trực tiếp (`>`) sẽ sai lệch nghiêm trọng khi giá trị đếm bị tràn qua số 0.
- Cách viết đúng: Sử dụng phép trừ không dấu (unsigned subtraction) để tự động xử lý hiện tượng tràn: 
  `while ((DWT->CYCCNT - start) < wait_cycles) { }`

## 5. Sửa starvation
Cách tôi chọn: Thêm `vTaskDelay(1)` vào cuối mỗi vòng lặp của task (ngay sau lệnh hạ chân GPIO).
Vì sao nó hoạt động (dựa vào trạng thái task trong scheduler):
- Khi gọi `vTaskDelay(1)`, task đang chạy sẽ chuyển từ trạng thái `Running` sang `Blocked` trong khoảng thời gian ít nhất là 1 tick. Điều này giải phóng CPU, buộc scheduler phải tìm task có độ ưu tiên cao tiếp theo trong hàng đợi Ready (kể cả các task có ưu tiên thấp hơn như A hoặc B) để thực thi, từ đó phá vỡ hoàn toàn sự độc chiếm CPU của task ưu tiên cao.
Đánh đổi: 
- Phát sinh overhead chuyển ngữ cảnh (context switch) liên tục.
- Lãng phí tài nguyên CPU do các task bị khóa cứng thời gian thực thi theo nhịp tick và chừa khoảng trống cho Idle task.

## 6. Lệch so với dự đoán
- Các kết quả đo đạc thực tế trên phần cứng đều bám sát và khớp hoàn toàn với các mốc thời gian cũng như lý thuyết đã dự đoán trước trong `PREDICTION.md`.

## 7. Câu hỏi đánh đổi
Với **bộ ưu tiên của bạn**, nếu `TASK_C_WORK_US` tăng gấp 3 lần thì bảng CPU share ở bước 2 thay đổi thế nào? Trả lời bằng số suy ra từ dữ liệu của bạn, rồi **đo lại để kiểm chứng**. Nếu lệch, giải thích.
- Trả lời: Do ở bước 2, Task A và Task B đã chịu hiện tượng starvation hoàn toàn (0% CPU share) và Task C chiếm trọn ~100% thời gian CPU, việc tăng `TASK_C_WORK_US` lên gấp 3 lần **không làm thay đổi** tỷ lệ CPU share (Task A: 0%, Task B: 0%, Task C vẫn chiếm 100%). Task C chỉ đơn giản là kéo dài thời gian thực thi bên trong vòng lặp của chính nó mà thôi.