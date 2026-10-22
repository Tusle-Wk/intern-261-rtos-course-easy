# Dự đoán — Week 1 / Ex4

> **Commit TRƯỚC khi nạp chương trình.**

`student_id`: 2413858    `HEAP_SIZE_BYTES` = 9216    `DYN_TASK_STACK_WORDS` = 160
`STATIC_TASK_STACK_WORDS` = 128    `HOG_DEPTH_BYTES` = 384    `TARGET_FREE_HEAP_BYTES` = 3072

1. Ngân sách RAM dự đoán (tổng phải = 20480 bytes):

| Mục | Bytes dự đoán |
|---|---|
| `.data` | 96 |
| `.bss` (không kể heap FreeRTOS) | 3344 |
| Heap FreeRTOS (`ucHeap`) | 9216 |
| Stack của main (MSP) | 1024 |
| Còn lại / chưa dùng | 6800 |
| **Tổng** | **20480** |

2. `xPortGetFreeHeapSize()` ngay sau khi scheduler chạy: ~7600 bytes.
   `HEAP_SIZE_BYTES` − số này = ~1616 bytes. Số đó là của những gì?
   - Là lượng RAM Heap mà FreeRTOS đã cấp phát động cho các task khởi tạo ban đầu (`DynTask`, `RamReport`), các khối điều khiển (TCB) và cấu trúc dữ liệu quản lý bộ nhớ ngầm của Kernel.

3. High water mark dự đoán (words): dyn ~140, stat ~120, idle ~120, timer ~240.

4. `dyn_task` có stack 160 words = 640 bytes. Mảng `HOG_DEPTH_BYTES` = 384 bytes.
   Hook tràn stack **có** chạy không? Vì sao?
   - Có chạy. Mảng HOG chiếm 384 bytes, làm dung lượng stack của task bị bóp nghẹt chỉ còn 256 bytes. Khoảng này không đủ để chứa context switch của Cortex-M3 (~64 bytes) cộng với lời gọi hàm `vTaskDelay`, dẫn đến vượt quá ranh giới stack và kích hoạt Hook.

5. Heap trong `.map` và `xPortGetFreeHeapSize()` sẽ lệch nhau. Lệch vì:
   > `.map` chỉ ra dung lượng phân bổ tĩnh được khai báo trước từ lúc biên dịch (`configTOTAL_HEAP_SIZE`), trong khi `xPortGetFreeHeapSize()` phản ánh con số động thay đổi thực tế trên RAM lúc chạy khi hệ điều hành xẻ thịt heap cấp phát cho các task.

6. Stack nhỏ nhất an toàn của `dyn_task` dự đoán: 80 words. Dựa vào đâu?
   > Dựa vào số lượng thanh ghi tối thiểu cần lưu khi ngắt Context Switch trên Cortex-M3 (16 words), kết hợp với độ sâu gọi hàm của `shared_work()` cộng thêm biên an toàn tối thiểu (32 words).
