/* Week 4 / Ex2 -- timer ISR that samples MPU6050 at SAMPLE_RATE_HZ and
 * feeds sample_queue.
 * Native FreeRTOS API only.
 */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "main.h"
#include "params.h"
#include "datalogger_types.h"
#include "mpu6050.h"
#include "sampler_isr.h"

#define SAMPLE_QUEUE_LENGTH  64

QueueHandle_t sample_queue;
TaskHandle_t sampler_task_handle;

static TIM_HandleTypeDef htim3;

void sampler_isr_init(void)
{
    sample_queue = xQueueCreate(SAMPLE_QUEUE_LENGTH, sizeof(sensor_sample_t)); //tạo queue chứa 64 phần tử, mỗi phần tử 16 byte

    //bật DWT để lấy tick thật
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    //config và khởi tạo TIM3
    __HAL_RCC_TIM3_CLK_ENABLE();
    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 7199;
    htim3.Init.Period = (10000 / SAMPLE_RATE_HZ) - 1;
    HAL_TIM_Base_Init(&htim3);

    //config ngắt cho TIM3
    HAL_NVIC_SetPriority(TIM3_IRQn, 7, 0);
    HAL_NVIC_EnableIRQ(TIM3_IRQn);
    HAL_TIM_Base_Start_IT(&htim3);
}

//handler IRQ của ngắt TIM3 mỗi 2ms
void TIM3_IRQHandler(void)
{
    if (__HAL_TIM_GET_FLAG(&htim3, TIM_FLAG_UPDATE)) {
        __HAL_TIM_CLEAR_FLAG(&htim3, TIM_FLAG_UPDATE);
        sampler_timer_callback();
    }
}

//hàm xử lý ngắt
void sampler_timer_callback(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    //đánh thức task bị block do chờ qua xTaskNotifyWait() hoặc ulTaskNotifyTake()
    xTaskNotifyFromISR(sampler_task_handle, DWT->CYCCNT, eSetValueWithOverwrite, &xHigherPriorityTaskWoken); 
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken); //nhảy qua task có độ ưu tiên cao hơn task hiện tại ngay nếu có
}
