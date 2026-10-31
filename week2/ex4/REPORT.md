# Report — Week 2 / Ex4

`student_id`: 2413858    Commit của PREDICTION.md: 897b64c

## 1. Bảng so sánh RAM
| | Bytes thực tế | Cách tính |
|---|---|---|
| Event group (3 điểm đồng bộ) | | |
| Event group (2 điểm) + notification (1 điểm) | | |
| Chênh lệch | | |

## 2. Bảng so sánh tốc độ đánh thức
| Cách | Thời gian đánh thức (cycle) | Thời gian đánh thức (us) |
|---|---|---|
| Event-group-bit (producer_1 hoặc 2) | | |
| Task notification (producer_0) | | |

Mô tả cách đo (2 điểm DWT: lúc gọi set/notify, lúc sync_task bắt đầu chạy):

## 3. Khi nào KHÔNG dùng được notification
Tình huống bạn dự đoán ở PREDICTION.md, kiểm chứng lại: đúng hay sai, vì sao.

## 4. Timeout
Log/ảnh một lần `sync_task` bị timeout (cố tình làm 1 producer chậm hơn
`SYNC_TIMEOUT_MS`), và giá trị trả về của `xEventGroupWaitBits` lúc đó.

## 5. Lệch so với dự đoán
Mỗi mục lệch + nguyên nhân vật lý.

## 6. Câu hỏi đánh đổi
Với `EVENT_BIT_COUNT` của **bạn**, nếu tăng thêm 1 bit (1 producer nữa) thì chu
kỳ đồng bộ trung bình đo được thay đổi thế nào? Trả lời bằng số từ dữ liệu của
bạn, rồi đo lại để kiểm chứng.

Dự đoán trước khi đo (N = 3 → 4): chu kỳ đồng bộ trung bình đo được ở N=3 là 7464.55 µs (Build A, 8000 lần đồng bộ). Mỗi producer ngủ ngẫu nhiên 5 đến 9 ms, và sync_task chỉ đồng bộ khi producer chậm nhất đã báo, nên chu kỳ gần bằng giá trị lớn nhất của N độ trễ ngẫu nhiên. Giá trị lớn nhất này tăng chậm vì bị chặn ở 9 ms. Mô phỏng Monte Carlo cho N=3 là 7.36 ms và N=4 là 7.47 ms, tức tăng khoảng 1.5%. Áp tỉ lệ đó vào số đo thực: 7464.55 × 1.015 ≈ 7.58 ms (tăng khoảng 0.11 ms). Dự đoán sai số trong khoảng ±0.1 ms.
