#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>

 //cấu hình phần cứng và xung nhịp
#define configCPU_CLOCK_HZ                      ( ( unsigned long ) 72000000 )
#define configTICK_RATE_HZ                      1000
#define configMAX_PRIORITIES                    5
#define configMINIMAL_STACK_SIZE                ( ( unsigned short ) 128 )
// #define configTOTAL_HEAP_SIZE                   ( ( size_t ) ( 10 * 1024 ) )   
#define configTOTAL_HEAP_SIZE                   ( ( size_t ) ( 9216 ) )   //ex4, heap bytes=9216
//#define configMAX_TASK_NAME_LEN                 16     //ex4, thêm mới: hooks.c dùng macro này để đọc tên task có giới hạn
//#define configSUPPORT_STATIC_ALLOCATION         1      //ex4, cấp phát tĩnh cho stat_task (SỬA: 0 -> 1)
//#define configCHECK_FOR_STACK_OVERFLOW          2      //ex4 (SỬA: 0 -> 2, bật kiểm tra tràn stack kiểu 2)
//#define configUSE_MALLOC_FAILED_HOOK            1      //ex4 (SỬA: 0 -> 1, bật hook hết heap)
#define INCLUDE_uxTaskGetStackHighWaterMark     1   //ex4
#define INCLUDE_xTaskGetIdleTaskHandle          1   // ex4 (Bật handle cho idle task)
#define INCLUDE_xTimerGetTimerDaemonTaskHandle  1   //ex4, thêm mới: lấy handle timer task để đọc HWM
//cơ chế hoạt động của Kernel
#define configUSE_PREEMPTION                    1 //test ex2
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configUSE_16_BIT_TICKS                  0
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configUSE_MUTEXES                       1  //week2 ex2  
//cấu hình bổ sung cho software timer
#define configUSE_TIMERS                        1      //ex4 (SỬA: 0 -> 1, cần timer task để đo HWM)
#define configTIMER_TASK_PRIORITY               ( 2 )
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            ( configMINIMAL_STACK_SIZE * 2 )


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