# Report — Week 1 / Ex4

`student_id`: 2413858    Commit của PREDICTION.md: fcf7f56

## 1. Ngân sách RAM từ `.map`
| Mục | Symbol / section trong `.map` | Bytes |
|---|---|---|
| `.data` | `.data` @ 0x20000000, kích thước 0x60 | 96 |
| `.bss` (không kể heap) | `.bss` @ 0x20000060, kích thước 0x3110 = 12560, trừ `.bss.ucHeap` 0x2400 = 9216 | 3344 |
| Heap FreeRTOS (`ucHeap`) | `.bss.ucHeap` @ 0x20000c0c, kích thước 0x2400 | 9216 |
| Stack của main (MSP) | `._user_heap_stack`: `_Min_Stack_Size` = 0x400 (0x20003370 → 0x20003770) | 1024 |
| Còn lại / chưa dùng | — `_Min_Heap_Size` = 0x200 (0x20003170 → 0x20003370, dành cho `_sbrk` của newlib) = 512, cộng vùng trống 0x20003770 → `_estack` 0x20005000 = 6288 | 6800 |
| **Tổng** | `_estack` = 0x20005000 = ORIGIN 0x20000000 + LENGTH 0x5000 | **20480** |

Chi tiết `.bss` 3344 bytes: static của app 600 (`xStatTaskTCB` 76, `xStatTaskStack` 512, 3 handle 12); static của idle/timer 1688 (`xIdleTaskTCB` 76, `uxIdleTaskStack` 512, `xTimerTaskTCB` 76, `uxTimerTaskStack` 1024); timer queue tĩnh 192 (`ucStaticTimerQueueStorage` 120, `xStaticTimerQueue` 72); biến kernel 328 (`tasks.c` 260, `timers.c` không kể queue 60, `port.c` 8); thống kê `heap_4` 28; HAL/main/crt/sysmem 180; newlib (`__sf`, `errno`, lock...) 328.

`.map` commit tại: `a893cce`

## 2. Số lúc chạy (dán nguyên văn log UART)
Cấu hình: `TRIGGER_OVERFLOW = 0`, `DYN_TASK_STACK_WORDS = 160` (baseline).
```
--- RAM REPORT ---
Free Heap: 7344 bytes (Min ever: 7344 bytes)
Target >= 3072 bytes: OK
HWM DynTask  : 131 words (524 bytes)
HWM StatTask : 103 words (412 bytes)
HWM RamReport: 78 words (312 bytes)
HWM IdleTask : 105 words (420 bytes)
HWM TimerTask: 212 words (848 bytes)

--- RAM REPORT ---
Free Heap: 7344 bytes (Min ever: 7344 bytes)
Target >= 3072 bytes: OK
HWM DynTask  : 131 words (524 bytes)
HWM StatTask : 103 words (412 bytes)
HWM RamReport: 78 words (312 bytes)
HWM IdleTask : 105 words (420 bytes)
HWM TimerTask: 212 words (848 bytes)

--- RAM REPORT ---
Free Heap: 7344 bytes (Min ever: 7344 bytes)
Target >= 3072 bytes: OK
HWM DynTask  : 131 words (524 bytes)
HWM StatTask : 103 words (412 bytes)
HWM RamReport: 78 words (312 bytes)
HWM IdleTask : 105 words (420 bytes)
HWM TimerTask: 212 words (848 bytes)
```

| Đại lượng | Words | Bytes |
|---|---|---|
| `xPortGetFreeHeapSize()` | — | 7344 |
| `xPortGetMinimumEverFreeHeapSize()` | — | 7344 |
| HWM `dyn_task` | 131 | 524 |
| HWM `stat_task` | 103 | 412 |
| HWM `ram_report_task` | 78 | 312 |
| HWM idle task | 105 | 420 |
| HWM timer task | 212 | 848 |

Heap còn trống >= `TARGET_FREE_HEAP_BYTES` (3072)? Có: 7344 >= 3072, dư 4272 bytes.

## 3. Đối chiếu `.map` vs runtime
Heap trong `.map` = 9216 ; `xPortGetFreeHeapSize()` = 7344 ; lệch = 1872 bytes.
Lệch này gồm những gì (liệt kê từng mục kèm số bytes):

| Mục | Bytes |
|---|---|
| `ucHeap` @ 0x20000c0c không chia hết cho 8, `heap_4` căn lên 0x20000c10 | 4 |
| Cuối heap: khối đánh dấu kết thúc 8 bytes và căn lề 8 (0x2000300c → 0x20003000) | 12 |
| TCB của `dyn_task`: 76 + header 8 = 84, làm tròn bội số 8 | 88 |
| Stack của `dyn_task`: 160 × 4 = 640 + header 8 | 648 |
| TCB của `ram_report_task`: 76 + header 8, làm tròn | 88 |
| Stack của `ram_report_task`: 256 × 4 = 1024 + header 8 | 1032 |
| **Cộng** | **1872** |

Kiểm tra: 9216 − 1872 = 7344, khớp đúng với `xPortGetFreeHeapSize()`. Stat task, idle task, timer task và timer queue dùng buffer static nằm trong `.bss` (đã thấy trong `.map`), nên không tốn heap và không nằm trong 1872 bytes này.

## 4. Dynamic vs static
HWM của `dyn_task` và `stat_task` lệch 28 words (131 so với 103) dù làm cùng một việc. Vì sao?
- Stack cấp phát khác nhau: 160 so với 128 words, lệch 32 words. Mức dùng thật chỉ lệch 4 words: `dyn_task` dùng 160 − 131 = 29 words, `stat_task` dùng 128 − 103 = 25 words. Vậy 28 = 32 − 4.
- Chênh 4 words đó không đến từ cách tạo task. Bằng chứng ở mục 8: `ExtraTask` tạo bằng `xTaskCreate` (động), chạy thẳng `shared_work` với stack 128 words, có HWM 103 words, đúng bằng `stat_task` (static). Giả thuyết hợp lý (chưa kiểm chứng bằng file `.lst`): `dyn_task_work` bọc thêm một lớp gọi `shared_work()` nên có thêm một frame hàm.

`xTaskCreate` và `xTaskCreateStatic` khác nhau ở đâu về **nơi** TCB và stack nằm?
- `xTaskCreate`: cả TCB lẫn stack được cấp bằng `pvPortMalloc` từ `ucHeap`, làm giảm heap (xem bảng mục 3) và có thể thất bại khi hết heap (trả `errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY`, gọi `vApplicationMallocFailedHook`). Trả về `BaseType_t` (`pdPASS` khi thành công).
- `xTaskCreateStatic`: TCB và stack do người dùng cung cấp từ trước. Ở đây là `xStatTaskTCB` (0x4c = 76 bytes) và `xStatTaskStack` (0x200 = 512 bytes = 128 words kiểu `StackType_t`) nằm trong `.bss`, cố định từ lúc link, không tốn heap. Trả về `TaskHandle_t` (NULL nếu buffer không hợp lệ), không phải `pdPASS`.

## 5. Tràn stack
Log/ảnh chứng minh hook đã chạy:
Cấu hình: `TRIGGER_OVERFLOW = 1`, `DYN_TASK_STACK_WORDS = 126`.
```
[FATAL] STACK OVERFLOW in task: DynTask
```
Ảnh chụp Serial Debug Assistant kèm theo trong repo cho thấy đúng dòng trên, với bộ đếm nhận 43 bytes, bằng độ dài một lần in `\r\n[FATAL] STACK OVERFLOW in task: DynTask\r\n`. Hook in đúng một lần rồi treo, và không có khối RAM REPORT nào xen vào.

Ngưỡng đã thử: với stack 160 words và `TRIGGER_OVERFLOW = 1`, hook không chạy (HWM `dyn_task` còn 37 words, nên task dùng 123 words). Thử 127 words thì hook không chạy (HWM 5 words), thử 126 words thì hook chạy. Lý do: `configCHECK_FOR_STACK_OVERFLOW = 2` kiểm tra 16 bytes (4 words) cuối stack còn nguyên mẫu hay không; task dùng 123 words nên cần stack >= 127 để 4 words cuối còn nguyên.

Tại sao trong `vApplicationStackOverflowHook` không được gọi hàm UART blocking
(hoặc: nếu bạn vẫn gọi được, giải thích tại sao nó không crash — và tại sao đó là may):

Hook của tôi không gọi `HAL_UART_Transmit`, mà ghi thẳng thanh ghi `USART1->DR` và chờ cờ `TXE`/`TC`. Lý do không dùng hàm UART blocking của HAL:
- Hook được gọi từ `vTaskSwitchContext` trong PendSV, trên MSP, lúc kernel đang dở chuyển ngữ cảnh. Không có task nào đang chạy hợp lệ, nên mọi hàm của RTOS đều không an toàn.
- `HAL_UART_Transmit` dùng `__HAL_LOCK` và kiểm tra `gState` của `huart1`. Nếu `ram_report_task` đang truyền dở khi hook chạy thì HAL trả `HAL_BUSY` và không in gì, nên dòng báo lỗi có thể mất.
- HAL còn dùng biến trạng thái chung (`huart1`) có thể đang ở trạng thái giữa chừng, và timeout của nó dựa vào `HAL_GetTick` (TIM4), trong khi hook đã `__disable_irq()` nên tick không tăng.
- Tên task `pcTaskName` có thể bị hỏng nên chỉ đọc có giới hạn `configMAX_TASK_NAME_LEN` và chỉ in ký tự in được.

## 6. Stack nhỏ nhất an toàn
| Lần thử | Stack (words) | HWM (words) | Kết quả |
|---|---|---|---|
| 1 | 160 | 131 | Đạt (dư nhiều, tham số gốc của bài) |
| 2 | 72 | 34 | Đạt (biên 34) |
| 3 | 67 | 39 | Đạt |
| 4 | 66 | 32 | Đạt sát biên (đúng bằng 32) |
| 5 | 65 | 26 | Không đạt biên (< 32) |

Giá trị cuối cùng chọn: 72 words. Biên an toàn: 34 words (HWM đo được ở log cuối với stack 72). Lý do chọn biên đó:
- Stack nhỏ nhất chỉ vừa đạt là 66 words (HWM đúng bằng 32), nhưng mức dùng stack của `dyn_task` không cố định giữa các lần build và lần chạy: 28 đến 39 words (stack 67 dùng 28, stack 66 dùng 34, stack 72 dùng 38, stack 65 dùng 39). Chọn 66 thì chỉ cần mức dùng tăng thêm 1 word là hết biên.
- Mức dùng tệ nhất đã quan sát là 39 words, nên 72 − 39 = 33 words, vẫn >= 32. Chạy lâu ở stack 72, HWM giữ ổn định ở 34 (log cuối) và 43 (log mục 8), không xuống dưới 32.
- Biên 32 words (128 bytes) đủ cho exception frame của ngắt (8 words), phần lưu ngữ cảnh của PendSV, và đường code chưa chạy tới trong lúc đo. 72 words = 288 bytes, là bội số của 8 nên không phí thêm do làm tròn của `heap_4`.

## 7. Lệch so với dự đoán
- Ngân sách `.map`: khớp. `.data` 96, `.bss` 3344, `ucHeap` 9216, MSP 1024, còn lại 6800 đều đúng. Lưu ý 6800 gồm 512 bytes `_Min_Heap_Size` (dự trữ cho `_sbrk` của newlib) và 6288 bytes thật sự chưa dùng, vì dự đoán chưa tách riêng 512 này.
- `xPortGetFreeHeapSize()`: dự đoán ~7600, thực tế 7344 (thấp hơn 256). Chênh lệch heap dự đoán ~1616, thực tế 1872 (cao hơn 256). Nguyên nhân: dự đoán chưa tính đủ stack 256 words của `ram_report_task` cùng header và căn lề 8 bytes của `heap_4` (chi tiết ở mục 3).
- HWM (words): dyn dự đoán ~140, thực tế 131; stat ~120, thực tế 103; idle ~120, thực tế 105; timer ~240, thực tế 212. Dự đoán đều cao hơn thực tế vì task dùng nhiều stack hơn dự tính: dyn dùng 29 words, stat 25, idle 23, timer 44 (gồm frame của kernel, PendSV và ngắt tick).
- Hook tràn stack ở 160 words: dự đoán "có chạy" là sai. Thực tế HWM 37 words, hook không chạy, vì `dyn_task` chỉ dùng 123 words trong 160 (640 bytes), còn dư 37 words. Dự đoán đã nhầm khi coi 256 bytes còn lại là không đủ, trong khi context switch chỉ cần khoảng vài chục words. Ngưỡng thực tế là 127 words (hook chạy khi stack <= 126).
- Lý do heap `.map` và runtime lệch: dự đoán đúng hướng (tĩnh so với động) nhưng chưa nêu từng mục. Thực tế lệch 1872 bytes gồm căn lề đầu và cuối heap (16), TCB và stack của hai task động kèm header (1856), như bảng ở mục 3.
- Stack nhỏ nhất dự đoán 80 words, thực tế đạt biên 32 từ 66 words và chọn 72 words. Lệch 8 đến 14 words vì dự đoán cộng cứng 16 (context) + độ sâu hàm + 32, trong khi mức dùng đo được chỉ 28 đến 39 words.

## 8. Câu hỏi đánh đổi
Với `HEAP_SIZE_BYTES` = 9216 của bạn: nếu phải thêm một task nữa có stack 128 words,
**lấy RAM từ đâu**? Dựa vào bảng ngân sách của bạn, đưa ra 2 phương án kèm chi phí
cụ thể (bytes), rồi chọn một và **thực hiện, đo lại để chứng minh**.

Xuất phát: stack `dyn_task` đã chốt 72 words, free heap 7696 bytes, vùng chưa dùng 6288 bytes. Task thêm cần stack 128 words = 512 bytes.

- **Phương án A: lấy từ heap**, tạo bằng `xTaskCreate`. Chi phí: TCB 76 + header 8 làm tròn = 88, cộng stack 512 + header 8 = 520, tổng **608 bytes** từ `ucHeap`. Free heap kỳ vọng 7696 − 608 = 7088 (vẫn >= 3072). Không cần vùng RAM mới, nhưng heap có thể bị phân mảnh về sau.
- **Phương án B: lấy từ RAM chưa dùng**, tạo bằng `xTaskCreateStatic` với buffer static trong `.bss`. Chi phí: TCB 76 + stack 512 = **588 bytes** lấy từ vùng chưa dùng 6288 bytes (còn khoảng 5700), heap giữ nguyên 7696. Không tốn heap và không phân mảnh, nhưng cố định ngay lúc link, không trả lại được.

Chọn phương án A vì heap còn dư nhiều (7096 so với ngưỡng 3072) và chứng minh gọn nhất: free heap phải giảm đúng bằng chi phí tính được. Đã thực hiện: thêm `ExtraTask` (`xTaskCreate`, stack 128 words, cùng `shared_work`) vào `app_main.c` và in HWM của nó trong `ram_report.c`.

Log sau khi thực hiện (`TRIGGER_OVERFLOW = 0`, stack `dyn_task` = 72, có `ExtraTask`):
```
--- RAM REPORT ---
Free Heap: 7096 bytes (Min ever: 7096 bytes)
Target >= 3072 bytes: OK
HWM DynTask  : 43 words (172 bytes)
HWM StatTask : 103 words (412 bytes)
HWM RamReport: 78 words (312 bytes)
HWM IdleTask : 106 words (424 bytes)
HWM TimerTask: 211 words (844 bytes)
HWM ExtraTask: 103 words (412 bytes)
```

| | Trước (chưa có ExtraTask) | Sau (có ExtraTask) |
|---|---|---|
| Free heap | 7696 | 7096 |
| Chênh lệch | | 600 bytes |
| HWM `ExtraTask` | — | 103 words (bằng `stat_task`) |
| HWM `dyn_task` | 34 words | 43 words (vẫn >= 32) |

Chênh lệch đo được là 600 bytes, không phải 608 như tính. 8 bytes lệch giải thích bằng `.map` của build này: biến `xExtraTaskHandle` (4 bytes, @ 0x2000036c) làm `ucHeap` dịch từ 0x20000c0c lên **0x20000c10**, chia hết cho 8 nên `heap_4` không còn mất 4 bytes căn lề ở đầu, và phần cuối cũng lấy lại 4 bytes, nên free ban đầu tăng từ 9200 lên 9208. Kiểm tra: 9208 − (88 + 296) − (88 + 1032) − 608 = 7096, khớp log. Task mới tốn đúng 608 bytes như tính. Trong build này `.bss` = 0x3114 = 12564 (không kể heap 3348, cộng 4 bytes đệm căn lề trước `_end` 0x20003178), vùng chưa dùng còn 6280 bytes (0x20003778 → 0x20005000), và bảng ngân sách vẫn cộng đúng 20480.

`.map` của build có ExtraTask (`ex4_extraTask_trigger0_stack72.map`) commit tại: `ff22b45`
