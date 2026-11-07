/* Week 3 / Ex2 -- software timer debounce.
 * Native FreeRTOS API only.
 */
#include "FreeRTOS.h"
#include "task.h"

#include "main.h"
#include "params.h"
#include "debounce.h"

#ifndef BTN_Pin
#define BTN_Pin        GPIO_PIN_0
#define BTN_GPIO_Port  GPIOA
#define BTN_EXTI_IRQn  EXTI0_IRQn
#endif

void app_main(void)
{
    /* TODO: GPIO init for the button pin as EXTI, both edges
     *       (GPIO_MODE_IT_RISING_FALLING) -- you need every bounce edge, not
     *       just the first one. */

    /* GPIO init cho chan nut la EXTI, CA HAI CANH -- can moi canh nang. */
    GPIO_InitTypeDef gpio = {0};
 
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();
    //config button pull up
    gpio.Pin  = BTN_Pin;
    gpio.Mode = GPIO_MODE_IT_RISING_FALLING;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(BTN_GPIO_Port, &gpio);
 
    //tạo timer trước khi bật ngắt, tránh cạnh đến sớm mà handle còn null
    debounce_init();
 
    /* prio ngắt phải >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY
        dùng 6 cho an toàn */
    HAL_NVIC_SetPriority(BTN_EXTI_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(BTN_EXTI_IRQn);

    vTaskStartScheduler();

    for (;;) {
    }
}

//có ngắt thì vào đây trước để xem ngắt ở chân nào
void EXTI0_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}
// gọi từ EXTIx_IRQHandler -> HAL_GPIO_EXTI_IRQHandler(BTN_Pin) (stm32f1xx_it.c).
 // Ghi đè hàm __weak cua HAL, gọi debounce_on_edge() tren mỗi cạnh
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == BTN_Pin) {
        debounce_on_edge((uint8_t)(HAL_GPIO_ReadPin(BTN_GPIO_Port, BTN_Pin) == GPIO_PIN_SET)); //ngắt chỉ biết là vừa đổi trạng thái, nên đọc tín hiệu chân ngay sau đó để biết cạnh lên hay xuống
    }
}