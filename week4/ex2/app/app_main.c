/* Week 4 / Ex2 -- data logger mini project, main implementation.
 * Native FreeRTOS API only.
 */
#include "FreeRTOS.h"
#include "task.h"

#include "main.h"
#include "params.h"
#include "mpu6050.h"
#include "sampler_isr.h"
#include "logger_task.h"

void app_main(void)
{
    mpu6050_init(); //khởi động MPU6050
    logger_init(); //tạo stream buffer, phải có trước khi các task chạy

    xTaskCreate(sampler_task, "sampler", 128, NULL, 3, &sampler_task_handle); //task lấy mẫu, priority 3, stack 128 words
    xTaskCreate(logger_task,  "proc",    128, NULL, 2, NULL); //task xử lý, priority 2
    xTaskCreate(uart_task,    "uart",    128, NULL, 1, NULL); //task ghi UART, priority 1

    sampler_isr_init(); //tạo queue, bật DWT và bật timer

    vTaskStartScheduler(); //bắt đầu chạy task

    for (;;) {
    }
}