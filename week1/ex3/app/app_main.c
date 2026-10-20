/* Week 1 / Ex3 — entry point. */
#include "FreeRTOS.h"
#include "task.h"

#include "main.h"
#include "params.h"

void periodic_task(void *argument);
void dwt_init(void);

//loi tran stack
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    for (;;) {
    }
}
void app_main(void)
{
    dwt_init();

    /* TODO: create periodic_task. Its priority must be the highest in the system
     *       — justify why in REPORT.md, and say what the measurement would look
     *       like if it were not. */
    BaseType_t ret = xTaskCreate(
        periodic_task,
        "Periodic",
        128,        //stack size
        NULL,
        configMAX_PRIORITIES - 1, //dam bao priority cao nhat
        NULL
    );
    //check coi du bo nho trong heap de cap cho task ret tren hay khong
    configASSERT(ret == pdPASS);

    vTaskStartScheduler();
    for (;;) {
    }
}
