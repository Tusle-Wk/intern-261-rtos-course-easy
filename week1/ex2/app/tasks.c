/* Week 1 / Ex2 — the three competing tasks.
 *
 * Each task: raise its pin, burn exactly TASK_x_WORK_US of CPU, lower its pin,
 * and loop again WITHOUT blocking. The missing block is the point of the
 * exercise, not a bug to fix in step 2.
 */
#include "FreeRTOS.h"
#include "task.h"

#include "main.h"
#include "params.h"
#include "tasks.h"

/* TODO: one helper that takes a port/pin and a work time, then three thin
 *       wrappers. Do not copy-paste the body three times. */


// hàm dùng chung cho 3 task
static void general_task(GPIO_TypeDef *port, uint16_t pin_num, uint32_t work_us){
    uint16_t hal_pin = (1 << pin_num);
    for (;;) {
        //kéo chân GPIO lên Mức HIGH
        HAL_GPIO_WritePin(port, hal_pin, GPIO_PIN_SET);
        
        //doi cpu (không block) 
        dwt_busy_wait_us(work_us);
        
        //hạ chân GPIO xuống Mức LOW
        HAL_GPIO_WritePin(port, hal_pin, GPIO_PIN_RESET);
        
        //fix starvation
        vTaskDelay(1);
    }
}

void task_a(void *argument) { general_task(GPIOA, TASK_A_PIN, TASK_A_WORK_US); }
void task_b(void *argument) { general_task(GPIOA, TASK_B_PIN, TASK_B_WORK_US); }
void task_c(void *argument) { general_task(GPIOA, TASK_C_PIN, TASK_C_WORK_US); }
