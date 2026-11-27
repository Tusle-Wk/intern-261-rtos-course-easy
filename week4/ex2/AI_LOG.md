# AI_LOG.md — Week 4 / Ex2

> Bắt buộc: ghi lại MỌI prompt đã dùng với AI khi làm bài này. Thiếu file
> này hoặc không tự giải thích được code mình nộp khi vấn đáp = phần vấn
> đáp của bài = 0 (xem SYLLABUS.md "Điểm liệt").

| # | Prompt (tóm tắt) | AI dùng | Mình giữ lại gì / sửa gì |
|---|---|---|---|
| 1 | Hỏi vì sao code AI gửi khác với code AI phân tích | Claude | Giữ bản viết lại theo đúng thiết kế ex1; bỏ include thừa (`<string.h>`, `mpu6050.h`) |
| 2 | Hỏi vòng lặp `while (got < BLOCK_BYTES)` trong `uart_task`, stream buffer khác `tx_block` thế nào, có mâu thuẫn ex1 không | Claude | Giữ vòng lặp; xác định đây là chi tiết hiện thực, không đổi kiến trúc so với ex1 |
| 3 | Nhờ rà soát code đã ổn chưa và cần test gì để làm report | Claude | Giữ code; thêm `extern I2C_HandleTypeDef hi2c1;` và `extern UART_HandleTypeDef huart1;` |
| 4 | Hỏi các bước hoàn thành report, từ config đến test để ước lượng | Claude | Làm theo các bước: sửa config, build, flash, test A, test B |
| 5 | Hỗ trợ sửa `FreeRTOSConfig.h`, `main.c`, đường dẫn trong `CMakeLists.txt` và rút gọn include | Claude | Dùng `FreeRTOSConfig.h` bản gọn; sửa CMake: thêm file nguồn của ex2 và include dir `week4/ex1/app`, `week4/ex2/app` |
| 6 | Hỏi cách test, nhờ viết script đo trên PC | Claude | Dùng `capture_check.py` (không nộp) để đếm byte, số khối/giây, mẫu mất |
| 7 | Gửi log lỗi build/link (thiếu `datalogger_types.h`, undefined reference kernel) | Claude | Thêm include path `week4/ex1/app`; thêm file kernel (tasks, list, queue, port, heap_4, stream_buffer) vào CMake |
| 8 | Test A: hỏi cách tái hiện lỗi `xQueueSend` trong ISR và xác nhận ảnh debugger | Claude | Dùng `sampler_isr_testA.c` chỉ để test (không nộp); dùng kết quả cho mục 1 report |
| 9 | Hỏi cách nối dây MPU6050 và xác nhận chân I2C1 | Claude | Nối PB6 (SCL), PB7 (SDA), VCC 3V3, AD0 xuống GND |
| 10 | Gửi kết quả capture 0 byte, lỗi `ninja: no work to do`, lỗi `hi2c1`/`huart1` chưa khai báo | Claude | Build lại bằng `--clean-first`, flash lại firmware đúng; thêm `extern` |
| 11 | Nhờ hướng dẫn đo thời gian truyền 1 khối UART và phân tích kết quả | Claude | Dùng `tx_cycles` (DWT) để đo, lấy số liệu cho mục 3, 4; xoá `tx_cycles` khỏi code nộp |
| 12 | Nhờ giải thích script Python và lệnh in dữ liệu cảm biến | Claude | Dùng để kiểm tra dữ liệu MPU6050 là thật; hiểu để trả lời vấn đáp |
| 13 | Hỗ trợ điền `REPORT.md` và `AI_LOG.md` | Claude | Tự điền hash commit PREDICTION; đối chiếu cột "Dự đoán" với `PREDICTION.md`; bổ sung các prompt đầu bài |