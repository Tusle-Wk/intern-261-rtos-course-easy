#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>
#include "params.h"                                     //MAX_HEAP_BYTES

//phần cứng và xung nhịp
#define configCPU_CLOCK_HZ                      ( ( unsigned long ) 72000000 )
#define configTICK_RATE_HZ                      1000
#define configMAX_PRIORITIES                    5
#define configMINIMAL_STACK_SIZE                ( ( unsigned short ) 128 )
#define configMAX_TASK_NAME_LEN                 16
#define configTOTAL_HEAP_SIZE                   ((size_t) MAX_HEAP_BYTES) //week4 rtosconfig
#define configUSE_TRACE_FACILITY                1 //week4 rtosconfig
#define configGENERATE_RUN_TIME_STATS           1 //week4 rtosconfig
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS()          //DWT đã bật trong sampler_isr_init()
#define portGET_RUN_TIME_COUNTER_VALUE()        (*(volatile uint32_t *)0xE0001004) //DWT->CYCCNT

//cấp phát bộ nhớ: queue, stream buffer, task đều cấp phát động
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configSUPPORT_STATIC_ALLOCATION         0

//các công tắc debug: TẮT để không cần hooks.c
#define configCHECK_FOR_STACK_OVERFLOW          0
#define configUSE_MALLOC_FAILED_HOOK            0
#define INCLUDE_uxTaskGetStackHighWaterMark     1

//cơ chế hoạt động của Kernel
#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                  1
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configUSE_16_BIT_TICKS                  0
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configUSE_TIMERS                        0      //ex2 không dùng software timer

//cấu hình NVIC Cortex-M3 (4 bits preemption)
#ifdef __NVIC_PRIO_BITS
    #define configPRIO_BITS                     __NVIC_PRIO_BITS
#else
    #define configPRIO_BITS                     4
#endif

#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY    5   //ngắt gọi API FromISR (TIM3 = 7) phải có số ưu tiên >= 5

#define configKERNEL_INTERRUPT_PRIORITY         ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

//assert kiểm tra runtime
#define configASSERT( x ) if( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for( ;; ); }

//API cần bật
#define INCLUDE_vTaskSuspend                    1      //để portMAX_DELAY chờ vô hạn (xQueueReceive, xStreamBufferReceive, xTaskNotifyWait)

//map 3 handler của port sang vector table
#define vPortSVCHandler                         SVC_Handler
#define xPortPendSVHandler                      PendSV_Handler
#define xPortSysTickHandler                     SysTick_Handler

#endif /* FREERTOS_CONFIG_H */