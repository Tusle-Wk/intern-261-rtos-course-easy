# Rubric — Week 1 / Ex2

| Phần | Trọng số | Đạt điểm khi |
|---|---|---|
| Code | **30%** | `check_api.sh` = 0, DWT busy-wait xử lý đúng wrap-around, không copy-paste 3 lần thân task, không dùng `HAL_Delay` |
| Số đo phần cứng | **30%** | Đủ 4 ảnh × 3 kênh, bảng CPU share có số thực, time slice đo được sai số < 5% |
| Giải thích | **25%** | Chỉ đúng >= 2 thời điểm preemption **ở bước 4** kèm dấu thời gian; giải thích đúng vì sao **bước 2 có 0 preemption** (starvation) theo trạng thái task; nói được đánh đổi của cách sửa |
| Vấn đáp | **15%** | Trả lời câu hỏi đánh đổi bằng số của mình và đã đo kiểm chứng |

Code tối đa 30%. Số đo + giải thích + vấn đáp = 70%.

## Trượt ngay nếu
- Có `cmsis_os*` / `os*` trong code.
- `PREDICTION.md` commit sau khi nạp chương trình.
- Bảng CPU share là số làm tròn đẹp (33/33/33) mà không có ảnh chứng minh.
- Ảnh thiếu kênh, hoặc không đọc được dấu thời gian.
