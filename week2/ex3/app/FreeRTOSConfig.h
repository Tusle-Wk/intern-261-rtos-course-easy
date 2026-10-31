#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>

/* Config RIÊNG cho week2/ex2 (copy từ week1/ex1, đã chỉnh).
 * Các dòng đánh dấu "ex2" là khác với bản gốc. */

//cấu hình phần cứng và xung nhịp
#define configCPU_CLOCK_HZ                      ( ( unsigned long ) 72000000 )
#define configTICK_RATE_HZ                      1000
#define configMAX_PRIORITIES                    5
#define configMINIMAL_STACK_SIZE                ( ( unsigned short ) 128 )
#define configTOTAL_HEAP_SIZE                   ( ( size_t ) ( 12288 ) )  //ex2: 12 kB, đủ cho cả 3 và 6 writer ở mọi mode
#define configMAX_TASK_NAME_LEN                 16

//cấp phát bộ nhớ: ex2 dùng xTaskCreate/xQueueCreate/xSemaphoreCreateMutex => động
#define configSUPPORT_DYNAMIC_ALLOCATION        1      //ex2: khai báo rõ
#define configSUPPORT_STATIC_ALLOCATION         0      //ex2: 1 -> 0 (nếu để 1 phải có vApplicationGetIdleTaskMemory)

//các công tắc debug của ex4: TẮT ở ex2 để không cần hooks.c
#define configCHECK_FOR_STACK_OVERFLOW          0      //ex2: 2 -> 0
#define configUSE_MALLOC_FAILED_HOOK            0      //ex2: 1 -> 0
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_xTaskGetIdleTaskHandle          1

//cơ chế hoạt động của Kernel
#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                  1      //ex2: ghi rõ; Mode 0 lẫn ký tự và dự đoán Mode 2 phụ thuộc vào đây
#define configUSE_MUTEXES                       1      //ex2: BẮT BUỘC cho Mode 1 (xSemaphoreCreateMutex)
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configUSE_16_BIT_TICKS                  0
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0

//software timer: ex2 không dùng => tắt (đỡ một task timer ~1 kB heap, không ảnh hưởng số đo)
#define configUSE_TIMERS                        0      //ex2: 1 -> 0

//cấu hình NVIC Cortex-M3 (4 bits preemption)
#ifdef __NVIC_PRIO_BITS
    #define configPRIO_BITS                     __NVIC_PRIO_BITS
#else
    #define configPRIO_BITS                     4
#endif

#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY    5

#define configKERNEL_INTERRUPT_PRIORITY         ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

//assert kiểm tra runtime
#define configASSERT( x ) if( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for( ;; ); }

//bật các hàm API FreeRTOS (bắt buộc cho app_main.c)
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskCleanUpResources           0
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1

//map 3 handler của port sang vector table
#define vPortSVCHandler                         SVC_Handler
#define xPortPendSVHandler                      PendSV_Handler
#define xPortSysTickHandler                     SysTick_Handler

#endif /* FREERTOS_CONFIG_H */