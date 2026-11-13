/* Week 3 / Ex3 -- UART RX -> stream buffer -> parser.
 * Native FreeRTOS API only.
 */
#include "FreeRTOS.h"
#include "task.h"

#include "main.h"
#include "params.h"
#include "uart_rx.h"
#include "parser_task.h"

/* Chon priority (configMAX_PRIORITIES = 5 -> 0..4):
1 byte o RX_BAUD = 115200, 8N1 mat 10 / 115200 = 86.8 us (~11520 byte/s).
 Parser xu ly 1 byte mat vai us => nhanh hon toc do den > 10 lan, mien la khong bi
task khac chen ngang qua STREAM_BUFFER_BYTES x 86.8 us = 192 x 86.8 us ~ 16.7 ms.
=> parser_task = 4 (cao nhat he thong). demo_task = 2 de lenh S co the doi len/xuong. */
void app_main(void)
{
    //khởi tạo uart
    uart_rx_init();
    //tạo 2 task
    xTaskCreate(parser_task, "parser", 256, NULL, configMAX_PRIORITIES - 1, NULL);
    xTaskCreate(demo_task, "demo", configMINIMAL_STACK_SIZE, NULL, 2, NULL);

    vTaskStartScheduler();

    for (;;) {
    }
}