/* Week 3 / Ex1 -- ISR -> task wakeup, NVIC priority.
 * Native FreeRTOS API only.
 */
#include "FreeRTOS.h"
#include "task.h"

#include "main.h"
#include "params.h"
#include "exti_button.h"

#define ACK_TASK_PRIO    (configMAX_PRIORITIES - 1u) //Prio cao nhất có thể
#define ACK_TASK_STACK   (128u)   /* words, task chỉ cần toggle 1 chân nên vậy đủ  */

void app_main(void)
{
    //cấu hình GPIO, EXTI, NVIC
    exti_button_init();

    BaseType_t rc = xTaskCreate(ack_task, "ack", ACK_TASK_STACK, NULL, ACK_TASK_PRIO, NULL);
    configASSERT(rc == pdPASS); //check đủ heap tạo task không
    (void)rc;
 
    vTaskStartScheduler();
 
    /* Only reached if the scheduler could not start (not enough heap). */
    for (;;) {
    }
}
