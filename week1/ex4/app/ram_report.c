/* Week 1 / Ex4 — print the live RAM picture over USART1.
 *
 * This is your measurement instrument. It must be honest: read the values from
 * FreeRTOS, never from a constant you typed in.
 */
#include "FreeRTOS.h"
#include "task.h"

#include "main.h"
#include "params.h"
#include "timers.h"
#include <stdio.h>
#include <string.h>

extern UART_HandleTypeDef huart1;
extern TaskHandle_t xDynTaskHandle;
extern TaskHandle_t xStatTaskHandle;
extern TaskHandle_t xRamReportTaskHandle;
extern TaskHandle_t xExtraTaskHandle; //ex4, mục 8 report
void app_fatal(const char *msg); //định nghĩa ở hooks.c

/* TODO 1: a blocking UART print helper. It is called from a task, so it may
 *         block — but say in REPORT.md what would break if you called it from
 *         the stack overflow hook instead. */

//hàm hỗ trợ in UART có block
static void uart_print(const char* str) {
    HAL_StatusTypeDef st = HAL_UART_Transmit(&huart1, (uint8_t*)str, strlen(str), HAL_MAX_DELAY);
    if (st != HAL_OK) { //check return value của HAL
        app_fatal("\r\n[FATAL] UART TX failed\r\n");
    }
}

void ram_report_task(void *argument)
{
    (void)argument;
    char buffer[200]; //mỗi lần in tối đa ~170 ký tự nên đủ, tách làm 2 lần in để không bị cắt cụt

    for (;;) {
        /* TODO 2: print, every 2 seconds:
         *   - xPortGetFreeHeapSize()
         *   - xPortGetMinimumEverFreeHeapSize()
         *   - uxTaskGetStackHighWaterMark() for: dyn_task, stat_task,
         *     ram_report_task, the idle task (xTaskGetIdleTaskHandle()),
         *     and the timer task (xTimerGetTimerDaemonTaskHandle()).
         *
         * High water mark is returned in WORDS. Print both words and bytes so
         * your report cannot silently mix the two units. */
        size_t free_heap = xPortGetFreeHeapSize(); // hàm trả về số byte RAM còn trống trong heap
        size_t min_free = xPortGetMinimumEverFreeHeapSize(); //hàm trả về số lượng byte trống ít nhất từng đuọc ghi nhận
        //check high water mark stack 3 của task đã tạo
        UBaseType_t hwm_dyn = uxTaskGetStackHighWaterMark(xDynTaskHandle); //số lượng stack sử dụng tối đa=số lượng words còn trống tối thiểu 
        UBaseType_t hwm_stat = uxTaskGetStackHighWaterMark(xStatTaskHandle);
        UBaseType_t hwm_rep = uxTaskGetStackHighWaterMark(xRamReportTaskHandle);
        //check stack 2 task ẩn
        UBaseType_t hwm_idle = uxTaskGetStackHighWaterMark(xTaskGetIdleTaskHandle());
        UBaseType_t hwm_timer = uxTaskGetStackHighWaterMark(xTimerGetTimerDaemonTaskHandle());

        //lần in 1: heap + so với TARGET_FREE_HEAP_BYTES để chứng minh bước 4
        snprintf(buffer, sizeof(buffer), //hàm ghi chuỗi vào buffer có limit max size
            "\r\n--- RAM REPORT ---\r\n"
            "Free Heap: %lu bytes (Min ever: %lu bytes)\r\n"
            "Target >= %u bytes: %s\r\n",
            (unsigned long)free_heap, (unsigned long)min_free,
            (unsigned)TARGET_FREE_HEAP_BYTES,
            (free_heap >= TARGET_FREE_HEAP_BYTES) ? "OK" : "FAIL");
        uart_print(buffer); //in ra 

        //lần in 2: HWM của 5 task
        snprintf(buffer, sizeof(buffer),
            "HWM DynTask  : %lu words (%lu bytes)\r\n"
            "HWM StatTask : %lu words (%lu bytes)\r\n"
            "HWM RamReport: %lu words (%lu bytes)\r\n"
            "HWM IdleTask : %lu words (%lu bytes)\r\n"
            "HWM TimerTask: %lu words (%lu bytes)\r\n",
            (unsigned long)hwm_dyn, (unsigned long)hwm_dyn * 4,//1 word=4 bytes theo kiến trúc ARM
            (unsigned long)hwm_stat, (unsigned long)hwm_stat * 4,
            (unsigned long)hwm_rep, (unsigned long)hwm_rep * 4,
            (unsigned long)hwm_idle, (unsigned long)hwm_idle * 4,
            (unsigned long)hwm_timer, (unsigned long)hwm_timer * 4);
        uart_print(buffer); //in ra
        //lần in 3: HWM của task thêm ở mục 8
        UBaseType_t hwm_extra = uxTaskGetStackHighWaterMark(xExtraTaskHandle);
        snprintf(buffer, sizeof(buffer),
            "HWM ExtraTask: %lu words (%lu bytes)\r\n",
            (unsigned long)hwm_extra, (unsigned long)hwm_extra * 4);
        uart_print(buffer);
        vTaskDelay(pdMS_TO_TICKS(2000));// ngủ 2 giây
    }
}