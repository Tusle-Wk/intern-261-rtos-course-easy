# Report — Week 1 / Ex1

`student_id`: 2413858    Commit của PREDICTION.md: 98ac2d9

## 1. Số đo
| Đại lượng | Giá trị |
|---|---|
| `BLINK_PERIOD_MS` (từ params.h) | |
| Chu kỳ đo được — min | 112.94ms |
| Chu kỳ đo được — max | 112.96ms |
| Chu kỳ đo được — trung bình | 112.95 ms |
| Sai số trung bình vs `BLINK_PERIOD_MS` | 0.05ms (50us) |
| `uxTaskGetStackHighWaterMark()` của bringup_task | 75 words |

Ảnh PulseView (>= 10 chu kỳ, đọc được giá trị): `![](do_ex1.png)`

## 2. Ba handler của kernel
| Tên trong port ARM_CM3 | Tên trong vector table | Nhiệm vụ |
|---|---|---|
| `vPortSVCHandler` | `SVC_Handler` | Gọi qua lệnh SVC để chuyển CPU sang privileged mode, khôi phục context và kích hoạt chạy task đầu tiên khi gọi `vTaskStartScheduler()`. |
| `xPortPendSVHandler` | `PendSV_Handler` | Thực hiện context switching giữa các task, chạy ở mức ưu tiên ngắt thấp nhất để không chặn ngắt ngoại vi phần cứng. |
| `xPortSysTickHandler` | `SysTick_Handler` | Đếm tick thời gian hệ thống, kiểm tra đánh thức các task đang blocked bởi `vTaskDelay` và kích hoạt cờ PendSV nếu cần đổi task. |

Log lỗi link trước khi sửa (dán nguyên văn):

```
C:/PROGRA~2/ARMGNU~1/14EFD8~1.2RE/bin/../lib/gcc/arm-none-eabi/14.2.1/../../../../arm-none-eabi/bin/ld.exe: CMakeFiles/template.dir/C_/Users/Dell/Documents/MLIoT-intern/intern-261-rtos-course-easy/third_party/FreeRTOS-Kernel/portable/GCC/ARM_CM3/port.c.obj: in function `SVC_Handler':
C:/Users/Dell/Documents/MLIoT-intern/intern-261-rtos-course-easy/third_party/FreeRTOS-Kernel/portable/GCC/ARM_CM3/port.c:230: multiple definition of `SVC_Handler'; CMakeFiles/template.dir/Core/Src/stm32f1xx_it.c.obj:C:/Users/Dell/Documents/MLIoT-intern/intern-261-rtos-course-easy/template/Core/Src/stm32f1xx_it.c:146: first defined here
C:/PROGRA~2/ARMGNU~1/14EFD8~1.2RE/bin/../lib/gcc/arm-none-eabi/14.2.1/../../../../arm-none-eabi/bin/ld.exe: CMakeFiles/template.dir/C_/Users/Dell/Documents/MLIoT-intern/intern-261-rtos-course-easy/third_party/FreeRTOS-Kernel/portable/GCC/ARM_CM3/port.c.obj: in function `PendSV_Handler':
C:/Users/Dell/Documents/MLIoT-intern/intern-261-rtos-course-easy/third_party/FreeRTOS-Kernel/portable/GCC/ARM_CM3/port.c:460: multiple definition of `PendSV_Handler'; CMakeFiles/template.dir/Core/Src/stm32f1xx_it.c.obj:C:/Users/Dell/Documents/MLIoT-intern/intern-261-rtos-course-easy/template/Core/Src/stm32f1xx_it.c:172: first defined here
C:/PROGRA~2/ARMGNU~1/14EFD8~1.2RE/bin/../lib/gcc/arm-none-eabi/14.2.1/../../../../arm-none-eabi/bin/ld.exe: CMakeFiles/template.dir/C_/Users/Dell/Documents/MLIoT-intern/intern-261-rtos-course-easy/third_party/FreeRTOS-Kernel/portable/GCC/ARM_CM3/port.c.obj: in function `SysTick_Handler':
C:/Users/Dell/Documents/MLIoT-intern/intern-261-rtos-course-easy/third_party/FreeRTOS-Kernel/portable/GCC/ARM_CM3/port.c:494: multiple definition of `SysTick_Handler'; CMakeFiles/template.dir/Core/Src/stm32f1xx_it.c.obj:C:/Users/Dell/Documents/MLIoT-intern/intern-261-rtos-course-easy/template/Core/Src/stm32f1xx_it.c:185: first defined here
```

Cách sửa:
Mở file `template/Core/Src/stm32f1xx_it.c`, xóa hoặc bọc comment quanh 3 hàm `SVC_Handler`, `PendSV_Handler` và `SysTick_Handler`. Việc này nhường quyền định nghĩa 3 vector ngắt cho file `port.c` của FreeRTOS Kernel, loại bỏ xung đột trùng lặp biểu tượng khi linker liên kết.

## 3. Tại sao HAL timebase phải sang TIM4
Phải trả lời được: `HAL_IncTick()` chạy ở đâu, `configKERNEL_INTERRUPT_PRIORITY`
ảnh hưởng gì đến nó, và `HAL_Delay()` / `vTaskDelay()` khác nhau chỗ nào.
1. `HAL_IncTick()` chạy ở đâu:
   Hàm này dùng để tăng biến đếm `uwTick` phục vụ các hàm timeout và `HAL_Delay()`. Ban đầu nó nằm trong `SysTick_Handler`. Khi đổi timebase sang TIM4, nó được gọi định kỳ 1 ms một lần trong `HAL_TIM_PeriodElapsedCallback()` thông qua ngắt `TIM4_IRQHandler`.

2. Ảnh hưởng của `configKERNEL_INTERRUPT_PRIORITY`:
   FreeRTOS cấu hình ngắt `SysTick` ở mức `configKERNEL_INTERRUPT_PRIORITY = 15` (mức ưu tiên thấp nhất trên Cortex-M3). 
   - Nếu giữ HAL timebase tại SysTick: Khi CPU bước vào một critical section hoặc đang xử lý một ngắt ngoại vi phần cứng (ISR có priority từ 0 đến 14) mà gọi hàm của HAL cần kiểm tra timeout hoặc `HAL_Delay()`, ngắt SysTick sẽ bị che/chặn hoàn toàn.
   - Hậu quả: `uwTick` không thể tăng, vòng lặp chờ của HAL (`while ((HAL_GetTick() - tickstart) < Wait)`) sẽ rơi vào trạng thái treo vĩnh viễn.
   - Khi chuyển sang TIM4 với mức ưu tiên ngắt cao hơn (mặc định mức 0), ngắt TIM4 không bị ảnh hưởng bởi critical section hay các ngắt khác của FreeRTOS, đảm bảo `uwTick` luôn tăng chuẩn xác.

3. Sự khác biệt giữa `HAL_Delay()` và `vTaskDelay()`:
   - `HAL_Delay(ms)`: Cơ chế Polling. CPU chạy vòng lặp vô tận liên tục so sánh biến `uwTick`, tiêu tốn 100% thời gian xử lý và không nhường quyền cho các task khác có độ ưu tiên thấp hơn chạy.
   - `vTaskDelay(ticks)`: Cơ chế Non-blocking dựa trên hệ điều hành. Task đang chạy sẽ tự động chuyển trạng thái sang `Blocked` và nhường CPU lại cho các task khác ở trạng thái Ready hoặc Idle Task. Khi hết khoảng thời gian delay, Scheduler sẽ tự động chuyển task trở lại hàng đợi Ready.

## 4. Lệch so với dự đoán
Với mỗi mục trong `PREDICTION.md` lệch so đo: lệch bao nhiêu, và **nguyên nhân vật lý**
(không được trả lời "do sai số").
- Dự đoán sai số 1-5 us, nhưng thực tế sai số là 50 us.
- Nguyên nhân vật lý:
  1. Sai số tần số của thạch anh phần cứng trên bo mạch Blue Pill khi đếm nhịp cho hệ thống suốt 113 ms tạo ra độ lệch cộng dồn.
  2. Thời gian vi điều khiển thực thi các tập lệnh ARM Assembly phục vụ cho việc lưu/khôi phục ngữ cảnh (context switch) trong ngắt `PendSV` và xử lý hàng đợi Ready trong `SysTick_Handler` làm thay đổi nhẹ điểm kích hoạt chân GPIO.

## 5. Câu hỏi đánh đổi
`configTICK_RATE_HZ` = 1000 trong bài này. Dựa vào số đo của **bạn**, nếu hạ xuống
100 Hz thì chu kỳ LED và sai số thay đổi thế nào? Trả lời bằng con số, không bằng lý thuyết.
- Chu kỳ LED thay đổi: Với cấu hình 100 Hz, 1 tick thời gian của FreeRTOS tương ứng với 10 ms. Hàm `vTaskDelay(pdMS_TO_TICKS(113))` (nếu viết đúng chuẩn) sẽ chuyển 113 ms thành 113 / 10 làm tròn xuống = 11 ticks. Chu kỳ thực tế LED sẽ chạy là 11 x 10 = 110 ms. Nếu viết cứng `vTaskDelay(113)`, chu kỳ sẽ thành 1130ms.
- Sai số thay đổi: Chu kỳ lý thuyết là 113 ms, chu kỳ thực tế điều khiển được là 110 ms. Sai số hệ thống sinh ra là 113 - 110 = 3ms (lớn gấp 60 lần sai số 0.05 ms đo được ở 1000 Hz).
