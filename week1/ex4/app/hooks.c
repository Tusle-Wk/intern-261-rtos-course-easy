/* Week 1 / Ex4 — FreeRTOS hooks.
 *
 * Both hooks must be observable from outside the chip. A breakpoint is not a
 * deliverable: you need a UART line or a distinct LED pattern you can photograph.
 */
#include "FreeRTOS.h"
#include "task.h"

#include "main.h"
#include "params.h"

#include <string.h>

extern UART_HandleTypeDef huart1;

//gửi từng byte thẳng vào thanh ghi USART1, không qua HAL
//vì HAL_UART_Transmit có lock, nếu ram_report_task đang truyền dở thì trả HAL_BUSY và không in gì
static void raw_send(const char *s, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        while (!(USART1->SR & USART_SR_TXE)) {}
        USART1->DR = (uint8_t)s[i];
    }
    while (!(USART1->SR & USART_SR_TC)) {}
}

//in lỗi rồi treo hệ thống, dùng được cả trước khi scheduler chạy
void app_fatal(const char *msg)
{
    __disable_irq();
    raw_send(msg, strlen(msg));
    for (;;) {}
}

// xử lý khi task vượt quá stack cho phép
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    /* TODO 1: report xTask / pcTaskName out of the chip, then stop.
     *
     * Careful: the stack of the offending task is already corrupt. Think about
     * what you are allowed to call here, and write the reasoning in REPORT.md.
     * Hint: this hook runs from the scheduler's context, not the task's.
     */
    //hook chạy trong PendSV (scheduler, stack MSP), kernel đang dở chuyển ngữ cảnh nên không gọi hàm RTOS
    //tên task có thể đã bị ghi đè nên chỉ đọc có giới hạn độ dài
    (void)xTask;
    __disable_irq();
    raw_send("\r\n[FATAL] STACK OVERFLOW in task: ", 34);
    if (pcTaskName != NULL) {
        for (size_t i = 0; i < configMAX_TASK_NAME_LEN; i++) {
            char c = pcTaskName[i];
            if (c == '\0') break;
            if (c < 0x20 || c > 0x7E) c = '?'; //ký tự hỏng thì thay bằng ?
            raw_send(&c, 1);
        }
    }
    raw_send("\r\n", 2);
    //treo hệ thống vĩnh viễn
    for (;;) {}
}
//heap hết ram để cấp cho task khi taskcreate
void vApplicationMallocFailedHook(void)
{
    /* TODO 2: report, then stop. */
    app_fatal("\r\n[FATAL] MALLOC FAILED\r\n");
}

/* configSUPPORT_STATIC_ALLOCATION = 1 makes these two callbacks mandatory.
 * Provide the memory for the idle task and the timer task here. */
//V10 dùng uint32_t, V11 dùng configSTACK_DEPTH_TYPE cho kích thước stack
#if defined(tskKERNEL_VERSION_MAJOR) && (tskKERNEL_VERSION_MAJOR >= 11)
typedef configSTACK_DEPTH_TYPE app_stack_size_t;
#else
typedef uint32_t app_stack_size_t;
#endif

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,     //tự cấp ram cho task idle
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   app_stack_size_t *pulIdleTaskStackSize)
{
    /* TODO 3: static buffers. Note the size is in WORDS, not bytes —
     *         getting this wrong is a silent overflow. */
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE; //tính bằng word
}

void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,   //tự cấp ram cho task timer
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    app_stack_size_t *pulTimerTaskStackSize)
{
    /* TODO 4: same, for the software timer task. */
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH; //tính bằng word
}
//hàm trống cho idle task
void vApplicationIdleHook(void)
{
}