#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>

//cấu hình phần cứng và xung nhịp
#define configCPU_CLOCK_HZ                      ( ( unsigned long ) 72000000 )
#define configTICK_RATE_HZ                      1000
#define configMAX_PRIORITIES                    5
#define configMINIMAL_STACK_SIZE                ( ( unsigned short ) 128 )
#define configTOTAL_HEAP_SIZE                   ( ( size_t ) ( 12288 ) )  //12 kB; week3/ex2 dùng ~3.3 kB (idle + timer task 2 kB + queue + 2 timer)
#define configMAX_TASK_NAME_LEN                 16

//cấp phát bộ nhớ: xTimerCreate dùng cấp phát động
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configSUPPORT_STATIC_ALLOCATION         0      //nếu để 1 phải có vApplicationGetIdleTaskMemory và vApplicationGetTimerTaskMemory

//các công tắc debug: TẮT để không cần hooks.c
#define configCHECK_FOR_STACK_OVERFLOW          0
#define configUSE_MALLOC_FAILED_HOOK            0
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_xTaskGetIdleTaskHandle          1

//cơ chế hoạt động của Kernel
#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                  1
#define configUSE_MUTEXES                       1
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configUSE_16_BIT_TICKS                  0      //tick 32 bit: xTaskGetTickCount() không tràn sớm
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0

//software timer: week3/ex2 BẮT BUỘC (xTimerCreate / xTimerResetFromISR / xTimerStartFromISR / xTimerStopFromISR)
#define configUSE_TIMERS                        1      //week3/ex2: 0 -> 1
#define configTIMER_TASK_PRIORITY               ( configMAX_PRIORITIES - 1 )  //= 4: daemon ưu tiên cao nhất, chạy ngay sau ISR
#define configTIMER_QUEUE_LENGTH                16     //mỗi cạnh nảy đẩy 2 lệnh (Reset + Start/Stop); queue đầy thì lệnh FromISR bị mất
#define configTIMER_TASK_STACK_DEPTH            512    //từ (word) = 2 kB: callback có printf (newlib) tốn nhiều stack hơn mặc định 128

//cấu hình NVIC Cortex-M3 (4 bits preemption)
#ifdef __NVIC_PRIO_BITS
    #define configPRIO_BITS                     __NVIC_PRIO_BITS
#else
    #define configPRIO_BITS                     4
#endif

#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY    5   //ngắt gọi API FromISR (EXTI0) phải có số ưu tiên >= 5; app_main.c dùng 6

#define configKERNEL_INTERRUPT_PRIORITY         ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

//assert kiểm tra runtime
#define configASSERT( x ) if( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for( ;; ); }

//bật các hàm API FreeRTOS
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskCleanUpResources           0
#define INCLUDE_vTaskSuspend                    1      //daemon cần để ngủ vô hạn khi không có timer nào chạy
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1      //chỉ dùng cho thí nghiệm bước 4; bản nộp cuối không gọi trong callback

//map 3 handler của port sang vector table
#define vPortSVCHandler                         SVC_Handler
#define xPortPendSVHandler                      PendSV_Handler
#define xPortSysTickHandler                     SysTick_Handler

#endif /* FREERTOS_CONFIG_H */