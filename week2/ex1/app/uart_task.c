/* Week 2 / Ex1 — prints filtered samples to USART1 at UART_BAUD.
 * queue_filtered -> UART.
 */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>

#include "main.h"
#include "params.h"
#include "pipeline.h"

extern UART_HandleTypeDef huart1;

volatile uint32_t uart_lost = 0;   //tổng số seq bị "lỗ" khi tới UART (mất ở bất kỳ tầng nào)
volatile uint32_t uart_tx_errors = 0;  //HAL_UART_Transmit trả về khác HAL_OK (timeout/lỗi)

void uart_task(void *argument)
{
    (void)argument;
    uint32_t expect_seq = 0;
    char line[40];                 //tối đa 10+1+6+1+6+1+6+2 = 33 byte + '\0'

    for (;;) {
        filtered_sample_t in;

        if (xQueueReceive(queue_filtered, &in, portMAX_DELAY) != pdPASS) {
            continue;
        }

        if (in.seq > expect_seq) {
            uart_lost += (in.seq - expect_seq);
        }
        expect_seq = in.seq + 1;

        int n = snprintf(line, sizeof line, "%lu,%d,%d,%d\r\n",
                         (unsigned long)in.seq, in.ax, in.ay, in.az);

        //Blocking polling TX: 1 byte = 10 bit / UART_BAUD.
        //230400 baud -> 43.4 us/byte;  9600 baud -> 1.042 ms/byte.
        //Timeout 100 ms > thời gian truyền 1 dòng ở 9600 (~30-34 ms)
        if (HAL_UART_Transmit(&huart1, (uint8_t *)line, (uint16_t)n, 100) != HAL_OK) {
            uart_tx_errors++;
        }
    }
}