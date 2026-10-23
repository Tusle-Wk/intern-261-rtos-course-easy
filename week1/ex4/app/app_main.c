/* Week 1 / Ex4 — entry point. */
#include "FreeRTOS.h"
#include "task.h"

#include "main.h"
#include "params.h"

void ram_report_task(void *argument);
void app_fatal(const char *msg); //định nghĩa ở hooks.c, in lỗi qua thanh ghi UART rồi treo

/* TODO 1: static storage for stat_task — TCB and stack.
 *         The stack array type matters; it is not char[]. */
static StaticTask_t xStatTaskTCB;
static StackType_t xStatTaskStack[STATIC_TASK_STACK_WORDS];

//khai báo handle để dùng ở file ram_report.c 
TaskHandle_t xDynTaskHandle = NULL;
TaskHandle_t xStatTaskHandle = NULL;
TaskHandle_t xRamReportTaskHandle = NULL;
TaskHandle_t xExtraTaskHandle = NULL; //ex4 mục 8, task thứ 4

// 0=chạy bình thường, 1=kích hoạt tràn stack bằng mảng HOG
// truyền bằng cờ build: -DTRIGGER_OVERFLOW=1 để truy vết được từng lần chạy

#ifndef DYN_STACK_USED
#define DYN_STACK_USED 72   //giá trị lựa chọn
#endif

#ifndef TRIGGER_OVERFLOW
#define TRIGGER_OVERFLOW 0
#endif

//stack của ram_report_task (words)
#define RAM_REPORT_STACK_WORDS 256

static void shared_work(void *argument)
{
    (void)argument;
    for (;;) {
        /* TODO 2: identical trivial work in both tasks, so that any difference
         *         in high water mark comes from how they were created, not from
         *         what they do. */
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
//
static void dyn_task_work(void *argument) {
#if TRIGGER_OVERFLOW
    //khai báo mảng nội bộ HOG_DEPTH_BYTES (384 bytes) để nhồi đầy stack
    //hog phải còn sống lúc gọi vTaskDelay, nếu không -O2 có thể giải phóng frame trước khi nhảy (tail-call)
    volatile uint8_t hog[HOG_DEPTH_BYTES];
    (void)argument;
    for (;;) {
        for (int i = 0; i < HOG_DEPTH_BYTES; i++) {
            hog[i] = 0xAA; //tránh bị compiler tối ưu hóa -O2 xóa mất mảng
        }
        vTaskDelay(pdMS_TO_TICKS(50));
        (void)hog[0]; //đọc lại để hog vẫn được dùng sau delay
    }
#else
    shared_work(argument);
#endif
}
void app_main(void)
{
    /* TODO 3: dyn_task  via xTaskCreate,       stack DYN_TASK_STACK_WORDS
     *         stat_task via xTaskCreateStatic, stack STATIC_TASK_STACK_WORDS
     *         ram_report_task via xTaskCreate
     *
     * Check every return value. xTaskCreateStatic returns a handle, not a
     * BaseType_t — explain the difference in REPORT.md. */

    /* TODO 4 (step 6 of the README, enable only for that run):
     *         declare a HOG_DEPTH_BYTES local array inside dyn_task's work
     *         function and write to every element, so the overflow is real and
     *         not optimised away. Think about what -O2 does to a dead array. */
    BaseType_t ret;

    //tạo dyn_task bằng cấp phát động (stack = 72 words, giá trị lựa chọn) 
    ret = xTaskCreate(dyn_task_work, "DynTask", DYN_STACK_USED, NULL, tskIDLE_PRIORITY+1, &xDynTaskHandle);
    if (ret != pdPASS) { //check heap đủ k (không dùng configASSERT vì có thể là no-op)
        app_fatal("\r\n[FATAL] xTaskCreate DynTask failed\r\n");
    }

    //tạo stat_task bằng cấp phát tĩnh (stack = 128 words)
    xStatTaskHandle = xTaskCreateStatic(shared_work, "StatTask", STATIC_TASK_STACK_WORDS, NULL, tskIDLE_PRIORITY+1, xStatTaskStack, &xStatTaskTCB);
    if (xStatTaskHandle == NULL) { //xTaskCreateStatic không trả về pdPass
        app_fatal("\r\n[FATAL] xTaskCreateStatic StatTask failed\r\n");
    }

    //tạo ram_report_task (cấp phát động, stack 256 words cho việc print)
    //ưu tiên +1 để không bị đói CPU bởi idle task
    ret = xTaskCreate(ram_report_task, "RamReport", RAM_REPORT_STACK_WORDS, NULL, tskIDLE_PRIORITY+1, &xRamReportTaskHandle);
    if (ret != pdPASS) { //check tràn heap
        app_fatal("\r\n[FATAL] xTaskCreate RamReport failed\r\n");
    }
    //tạo extra_task (cấp phát động, stack 128 words, cùng việc với 2 task kia)
    ret = xTaskCreate(shared_work, "ExtraTask", 128, NULL, tskIDLE_PRIORITY+1, &xExtraTaskHandle);
    if (ret != pdPASS) { //check heap đủ k
        app_fatal("\r\n[FATAL] xTaskCreate ExtraTask failed\r\n");
    }
    vTaskStartScheduler();
    /* Scheduler returned => not enough heap for the idle/timer task. */
    app_fatal("\r\n[FATAL] Scheduler returned\r\n");
}