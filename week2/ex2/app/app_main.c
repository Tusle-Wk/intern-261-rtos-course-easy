/* Week 2 / Ex2 — race condition & mutex.
 * Native FreeRTOS API only.
 */
#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>

#include "main.h"
#include "params.h"
#include "uart_guard.h"

void writer_task(void *argument);

/* Ưu tiên:
 *  Các writer CÙNG ưu tiên (1) và FreeRTOS time slicing đang bật => chúng
 * luân phiên nhau mỗi tick (1 ms). Đó là điều kiện để lỗi lẫn ký tự ở Mode 0
 * tái hiện ổn định và để so sánh công bằng giữa các mode.
 *  Gatekeeper (GATEKEEPER_PRIO = 3) cao hơn writer: ngay khi có bản tin nó
 * giành CPU và in, writer chỉ mất thời gian bỏ vào queue.
 * probe_task cao nhất (4): chu kỳ 1 ms, dùng để đo "task khác" bị ảnh hưởng
 * thế nào (nó bị trễ khi ngắt bị che). */
#define WRITER_PRIO         (tskIDLE_PRIORITY + 1)
#define PROBE_PRIO          (tskIDLE_PRIORITY + 4)
#define WRITER_STACK_WORDS  (256)
#define PROBE_STACK_WORDS   (128)
#define PROBE_PERIOD_MS     (1)
#define PROBE_LATE_US       (1500)   //chu kỳ thực > 1,5 ms coi là trễ

//số đo đọc bằng ưatch khi pause lúc debug
volatile uint32_t g_probe_n       = 0;  //số lần probe chạy
volatile uint32_t g_probe_max_us  = 0;  //khoảng cách lớn nhất giữa 2 lần probe chạy (lý tưởng 1000)
volatile uint32_t g_probe_late    = 0;  // số lần khoảng cách > PROBE_LATE_US
volatile int32_t  g_tick_lag_ms   = 0;  // thời gian thật trừ cho thời gian tick FreeRTOS, > 0 nghĩa là mất tick
volatile uint32_t g_guard_heap_bytes = 0; // RAM heap mà cơ chế bảo vệ chiếm (mutex / queue+task)

//task đo: ghi lại việc nó bị chậm lại bao nhiêu khi UART đang được in.
static void probe_task(void *argument)
{
    (void)argument;
    const uint32_t cyc_per_us = SystemCoreClock / 1000000U;
    const uint32_t cyc_per_ms = SystemCoreClock / 1000U;

    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t tick0 = last_wake;
    uint32_t prev = DWT->CYCCNT;
    uint64_t elapsed_cyc = 0;

    for (;;) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(PROBE_PERIOD_MS));

        const uint32_t now = DWT->CYCCNT;
        const uint32_t dt_cyc = now - prev;
        const uint32_t dt_us = dt_cyc / cyc_per_us;
        prev = now;
        elapsed_cyc += dt_cyc;

        g_probe_n++;
        if (dt_us > g_probe_max_us) {
            g_probe_max_us = dt_us;
        }
        if (dt_us > PROBE_LATE_US) {
            g_probe_late++;
        }
        g_tick_lag_ms = (int32_t)(elapsed_cyc / cyc_per_ms) - (int32_t)(xTaskGetTickCount() - tick0);//thời gian thật từ DWT trừ cho thời gian fake do bị mất tick ->lag time
    }
}

void app_main(void)
{
    //RAM mà cơ chế bảo vệ lấy từ heap: 0 ở Mode 0/2, mutex ở Mode 1, queue + TCB +
    //stack gatekeeper ở Mode 3
    vPortFree(pvPortMalloc(1));   //ép heap_4 khởi tạo trước khi đo
    const size_t heap_before = xPortGetFreeHeapSize();
    uart_guard_init();
    g_guard_heap_bytes = (uint32_t)(heap_before - xPortGetFreeHeapSize());

    // WRITER_COUNT task từ CÙNG một hàm, mỗi task nhận chỉ số của nó qua tham số
    configASSERT(WRITER_COUNT <= 10);
    for (uint32_t i = 0; i < WRITER_COUNT; i++) {
        char name[] = "writer0";
        name[6] = (char)('0' + i);
        if (xTaskCreate(writer_task, name, WRITER_STACK_WORDS, (void *)(uintptr_t)i, WRITER_PRIO, NULL) != pdPASS) {
            Error_Handler();
        }
    }

    if (xTaskCreate(probe_task, "probe", PROBE_STACK_WORDS, NULL, PROBE_PRIO, NULL) != pdPASS) {
        Error_Handler();
    }

    vTaskStartScheduler();

    for (;;) {
    }
}