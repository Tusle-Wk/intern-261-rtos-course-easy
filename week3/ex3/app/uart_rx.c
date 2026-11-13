/* Week 3 / Ex3 -- UART RX interrupt -> stream buffer.
 * Native FreeRTOS API only.
 */
#include "FreeRTOS.h"
#include "task.h"
#include "stream_buffer.h"

#include "main.h"
#include "params.h"
#include "uart_rx.h"

extern UART_HandleTypeDef huart1;

StreamBufferHandle_t rx_stream; //handle của stream buffer

// số byte mất do stream buffer đầy (SendFromISR trả về 0), xem trên debugger
volatile uint32_t uart_rx_dropped = 0;

static uint8_t s_rx_byte; //ô nhớ 1 byte, ghi byte vừa nhận từ uart vào đây

void uart_rx_init(void)
{
    //tạo stream buffer 192 byte, trigger level 4, cấp phát từ heap của FreeRTOS
    rx_stream = xStreamBufferCreate(STREAM_BUFFER_BYTES, TRIGGER_LEVEL); 
    //check heap coi đủ cấp không
    configASSERT(rx_stream != NULL);

    // ngắt usart1 phải có prio >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY thì mới gọi được API có đuôi FromISR
    HAL_NVIC_SetPriority(USART1_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);

    HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1); //bật ngắt chờ nhận 1 byte từ uart1
}

void uart_rx_isr_handler(void)
{
    //các hàm FromISR sẽ bật cờ này nếu chúng đánh thức một task có priority cao hơn task đang bị ngắt
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    //chép 1 byte vào buffer
    if (xStreamBufferSendFromISR(rx_stream, &s_rx_byte, 1, &xHigherPriorityTaskWoken) == 0) {
        uart_rx_dropped++;   // buffer đầy-> byte này mất, buffer không chờ
    }

    //nếu byte nhận được là \r thì gọi hàm dưới, ép đánh thức parser do triggerlevel là 4 (tối thiểu 4 byte mới chạy) để tránh ngủ mãi
    if (s_rx_byte == '\r') {
        xStreamBufferSendCompletedFromISR(rx_stream, &xHigherPriorityTaskWoken);
    }
    //arm lại để nhận byte kế
    HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1);

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken); //nếu cờ bật thì chuyển context sang hàm parse ngay sau khi thoát ngắt -> không phải chờ đến tick kế -> giảm delay
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        uart_rx_isr_handler(); //chạy hàm ở trên trong hàm xử lý ngắt uart1
    }
}

//hàm ứng với vector ngắt USART, xoá nếu lúc build bị lỗi multiple definition
#ifndef UART_RX_USE_CUBE_IRQ_HANDLER
void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}
#endif