# Report — Week 1 / Ex2

`student_id`: ______    Commit của PREDICTION.md: ______

## 1. CPU share đo được
| Cấu hình | A (%) | B (%) | C (%) | Time slice đo được | Ảnh |
|---|---|---|---|---|---|
| B2: preempt=1, ưu tiên khác nhau | | | | — | |
| B3: preempt=0, ưu tiên khác nhau | | | | — | |
| B4: preempt=1, cùng ưu tiên | | | | ms | |
| B5: sau khi sửa starvation | | | | | |

Cách tính CPU share từ ảnh PulseView (mô tả ngắn):

## 2. Preemption nhìn thấy ở đâu
Trên ảnh **bước 4** (cùng ưu tiên, round-robin), chỉ ra ít nhất **2 thời điểm**
preemption xảy ra. Với mỗi thời điểm: dấu thời gian, task bị cắt, task chiếm CPU,
và vì sao (hết time slice, không phải block).

| t (us) | Task bị preempt | Task chiếm CPU | Lý do |
|---|---|---|---|
| | | | |

Ở **bước 2** (ưu tiên khác nhau, đều busy-loop không block): giải thích vì sao ảnh
của bạn cho thấy **0 thời điểm preemption**, dù chạy bao lâu đi nữa — dựa vào điều
kiện preemption của FreeRTOS, không phải "vì ảnh chụp chưa đủ dài".

## 3. Time slice
Time slice đo được ở bước 4: ______ ms. `configTICK_RATE_HZ` = ______.
Sai số: ______ %. Giải thích sai số:

## 4. DWT wrap-around
Tại sao so sánh `DWT->CYCCNT > target` bị sai sau ~59 giây, và cách viết đúng:

## 5. Sửa starvation
Cách tôi chọn: ______
Vì sao nó hoạt động (dựa vào trạng thái task trong scheduler):
Đánh đổi: ______

## 6. Lệch so với dự đoán
Mỗi mục lệch + nguyên nhân vật lý.

## 7. Câu hỏi đánh đổi
Với **bộ ưu tiên của bạn**, nếu `TASK_C_WORK_US` tăng gấp 3 lần thì bảng CPU share ở
bước 2 thay đổi thế nào? Trả lời bằng số suy ra từ dữ liệu của bạn, rồi **đo lại để
kiểm chứng**. Nếu lệch, giải thích.
