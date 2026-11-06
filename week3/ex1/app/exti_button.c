/* Week 3 / Ex1 -- EXTI button wakes a task via task notification.
 * Native FreeRTOS API only.
 */
#include "FreeRTOS.h"
#include "task.h"

#include "main.h"
#include "params.h"
#include "exti_button.h"

/* README step 4: set EXTI_WRONG_PRIO_TEST to 1, build, flash and capture the
 * configASSERT. The submitted version MUST keep it at 0.
 * EXTI_WRONG_PRIO_VALUE must be SMALLER than
 * configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY (smaller = higher priority). */
#define EXTI_WRONG_PRIO_TEST    (0)
#define EXTI_WRONG_PRIO_VALUE   (2u)

static TaskHandle_t s_ack_task_handle;

void exti_button_init(void)
{
    /* TODO 1: GPIO init for PB<EXTI_PIN> as EXTI falling edge (GPIO_MODE_IT_FALLING,
     *         pull-up), and PA<ACK_PIN> as push-pull output.
     *
     * TODO 2 (do this FIRST, on purpose): HAL_NVIC_SetPriority(EXTIx_IRQn, <n>, 0)
     *         with <n> deliberately SMALLER (= higher hardware priority) than
     *         configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY. Run step 4 of the
     *         README with this wrong value first and capture the configASSERT.
     *         Only after that, set it to ISR_NVIC_PRIO and HAL_NVIC_EnableIRQ. */
    GPIO_InitTypeDef gpio = {0};
 
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
 
    /* PA<ACK_PIN>: push-pull output, idle low. */
    gpio.Pin   = (uint32_t)(1u << ACK_PIN);
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);
    HAL_GPIO_WritePin(GPIOA, (uint16_t)(1u << ACK_PIN), GPIO_PIN_RESET);
 
    /* PB<EXTI_PIN>: EXTI on falling edge, pull-up (button pulls it to GND). */ //input pull up
    gpio.Pin  = (uint32_t)(1u << EXTI_PIN);
    gpio.Mode = GPIO_MODE_IT_FALLING;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &gpio);
 
    /* PB11 -> EXTI line 11 -> shared vector EXTI15_10_IRQn. */
    //Chọn priority lúc biên dịch, nếu EXTI_WRONG_PRIO_TEST là 1 (để test trường hợp sai) thì dùng prio là 2, ngược lại thì  là 7 (đúng default)
#if EXTI_WRONG_PRIO_TEST
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, EXTI_WRONG_PRIO_VALUE, 0); //prio 2
#else
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, ISR_NVIC_PRIO, 0); //prio 7
#endif
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn); //cho phép NVIC nhận ngắt này
}

//hàm xử lý ngắt exti
void exti_button_isr_handler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    TaskHandle_t h = s_ack_task_handle; //đọc handle ra biến cục bộ một lần, để kiểm tra và dùng cùng một giá trị.
 
    /* The IRQ is enabled before the scheduler starts, so a very early press
     * could arrive while the handle is still NULL. */
    //chỉ gọi notify khi ack_task đã gán handle
    if (h != NULL) {
        // hàm này tăng bộ đếm notification của task. Nếu task đang block thì chuyển nó sang ready
        // và nếu task đó ưu tiên hơn task đang chạy thì ghi pdTRUE vào biến, đây cũng là nơi configASSERT kiểm tra priority.
        vTaskNotifyGiveFromISR(h, &xHigherPriorityTaskWoken);
    }
 
    //xoá yield khi test trường hợp không yield
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken); //nếu pdTRUE thì đặt cờ PendSV để context switch cho task, nhưng prio là 15 (thấp nhất) nên ngắt phải xong thì mới làm cái này
}

void ack_task(void *argument)
{
    (void)argument;
    s_ack_task_handle = xTaskGetCurrentTaskHandle();
 
    for (;;) {
        /* Block until the ISR gives a notification; pdTRUE clears the count. */
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
 
        /* Nothing between the wake-up and this toggle: this edge is the
         * end point of the measured latency. */
        HAL_GPIO_TogglePin(GPIOA, (uint16_t)(1u << ACK_PIN));
    }
}
