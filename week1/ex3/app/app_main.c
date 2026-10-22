/* Week 1 / Ex3 — entry point. */
#include "FreeRTOS.h"
#include "task.h"

#include "main.h"
#include "params.h"

void periodic_task(void *argument);
void dwt_init(void);

/* Dinh nghia MEASURE_TICK_OVERHEAD (-DMEASURE_TICK_OVERHEAD) de do muc 6 cua
 * REPORT.md: bo periodic task, chi chay task dem vong lap o uu tien thap nhat. */
#ifdef MEASURE_TICK_OVERHEAD
volatile uint32_t g_idle_counter = 0;       /* dem vong lap */
volatile uint32_t g_idle_per_sec = 0;       /* ket qua moi giay, doc bang debugger */

static void counter_task(void *arg)
{
    (void)arg;
    for (;;) {
        g_idle_counter++;
    }
}

static void monitor_task(void *arg)
{
    (void)arg;
    TickType_t last = xTaskGetTickCount();
    uint32_t prev = 0;
    for (;;) {
        xTaskDelayUntil(&last, pdMS_TO_TICKS(1000));
        uint32_t now = g_idle_counter;
        g_idle_per_sec = now - prev;
        prev = now;
    }
}
#endif

void app_main(void)
{
    dwt_init();

#ifdef MEASURE_TICK_OVERHEAD
    xTaskCreate(counter_task, "cnt", 128, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(monitor_task, "mon", 128, NULL, configMAX_PRIORITIES - 1, NULL);
#else
    /* Uu tien cao nhat he thong: khong task nao preempt duoc no, nen thoi diem
     * toggle chi phu thuoc vao SysTick. Neu thap hon, khi co task uu tien cao
     * hon dang chay thi periodic task phai cho trong hang Ready -> cac canh
     * bi tre bat thuong, max tang vot, jitter xau di (min/tb it doi). */
    BaseType_t ok = xTaskCreate(periodic_task, "periodic", 256, NULL,
                                configMAX_PRIORITIES - 1, NULL);
    configASSERT(ok == pdPASS);
    (void)ok;
#endif
    vTaskStartScheduler();

    for (;;) {
    }
}