/* Week 3 / Ex4 -- task_alpha and task_beta share two resources, protected by
 * two mutexes, mimicking a sensor register + a shared UART buffer that both
 * tasks occasionally need at once. This file is provided complete, not a
 * skeleton: build and run it exactly as given first and use
 * uxTaskGetSystemState() to find out why it stops making progress, before
 * you change anything in here. Native FreeRTOS API only.
 */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "main.h"
#include "params.h"
#include "deadlock_demo.h"
#include "supervisor.h"

static SemaphoreHandle_t s_mutex_a;
static SemaphoreHandle_t s_mutex_b;

void deadlock_demo_init(void)
{
    s_mutex_a = xSemaphoreCreateMutex();
    s_mutex_b = xSemaphoreCreateMutex();
    configASSERT(s_mutex_a != NULL);
    configASSERT(s_mutex_b != NULL);
}

void task_alpha(void *argument)
{
    (void)argument;

    for (;;) {
        xSemaphoreTake(s_mutex_a, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(5)); /* simulated register access time */
        xSemaphoreTake(s_mutex_b, portMAX_DELAY);

        /* ... shared work using both resources ... */

        xSemaphoreGive(s_mutex_b);
        xSemaphoreGive(s_mutex_a);

        supervisor_checkin(SUPERVISOR_BIT_ALPHA);
 
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void task_beta(void *argument)
{
    (void)argument;

    for (;;) {
        //xSemaphoreTake(s_mutex_b, portMAX_DELAY);
        //vTaskDelay(pdMS_TO_TICKS(5)); /* simulated UART buffer access time */
        //xSemaphoreTake(s_mutex_a, portMAX_DELAY);

        /* ... shared work using both resources ... */

        //xSemaphoreGive(s_mutex_a);
        //xSemaphoreGive(s_mutex_b);

        //fixed
        xSemaphoreTake(s_mutex_a, portMAX_DELAY);          /* đổi từ b sang a */
        vTaskDelay(pdMS_TO_TICKS(5));
        xSemaphoreTake(s_mutex_b, portMAX_DELAY);          /* đổi từ a sang b */
        
        xSemaphoreGive(s_mutex_b);                         /* nhả b trước */
        xSemaphoreGive(s_mutex_a);                         /* rồi a */


        supervisor_checkin(SUPERVISOR_BIT_BETA);

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
