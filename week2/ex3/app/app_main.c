/* Week 2 / Ex3 — priority inversion.
 * Native FreeRTOS API only.
 */
#include "FreeRTOS.h"
#include "task.h"

#include "main.h"
#include "params.h"
#include "tasks_lmh.h"

/* Priorities: idle = 0 (kernel), low = 1, mid = 2, high = 3.
 * Three distinct, consecutive values: low < mid < high, all > idle, and
 * high (3) < configMAX_PRIORITIES, so there is room above and no collision
 * with the idle task or any other task. Mid sits strictly BETWEEN low and
 * high — that is the condition that lets mid preempt low while high waits. */
#define PRIO_LOW   (tskIDLE_PRIORITY + 1)
#define PRIO_MID   (tskIDLE_PRIORITY + 2)
#define PRIO_HIGH  (tskIDLE_PRIORITY + 3)

_Static_assert(PRIO_HIGH < configMAX_PRIORITIES, "configMAX_PRIORITIES must be > 3 for this exercise");

#define TASK_STACK_WORDS  (configMINIMAL_STACK_SIZE * 2)

void app_main(void)
{
    resource_init();

    /* xTaskCreate task_low, task_mid, task_high with three DISTINCT
     * priorities, low < mid < high. */
    if (xTaskCreate(task_low,  "low",  TASK_STACK_WORDS, NULL, PRIO_LOW,  NULL) != pdPASS) {
        for (;;) {
        }
    }
    if (xTaskCreate(task_mid,  "mid",  TASK_STACK_WORDS, NULL, PRIO_MID,  NULL) != pdPASS) {
        for (;;) {
        }
    }
    if (xTaskCreate(task_high, "high", TASK_STACK_WORDS, NULL, PRIO_HIGH, NULL) != pdPASS) {
        for (;;) {
        }
    }

    vTaskStartScheduler();

    for (;;) {
    }
}