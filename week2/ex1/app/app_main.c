/* Week 2 / Ex1 — sensor pipeline: sensor_task -> queue_raw -> filter_task ->
 * queue_filtered -> uart_task.
 *
 * Native FreeRTOS API only.
 */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "main.h"
#include "params.h"
#include "pipeline.h"

QueueHandle_t queue_raw;
QueueHandle_t queue_filtered;

/* Ưu tiên: sensor là task duy nhất có deadline cứng (chu kỳ lấy mẫu) -> cao nhất.
 * uart_task dùng TX polling (busy-wait) và là "sink" chậm -> thấp nhất, để khi nó
 * chậm nó không cướp CPU của sensor/filter. Logger nằm trên uart để không bị đói
 * khi uart_task chiếm CPU ở 9600 baud, nhưng chạy vài µs/ms nên không làm nhiễu. */
#define PRIO_UART    (tskIDLE_PRIORITY + 1)
#define PRIO_LOGGER  (tskIDLE_PRIORITY + 2)
#define PRIO_FILTER  (tskIDLE_PRIORITY + 3)
#define PRIO_SENSOR  (tskIDLE_PRIORITY + 4)

//depth logger: ghi độ đầy 2 queue vào RAM, đọc ra bằng debugger
#define LOG_PERIOD_MS  (1)
#define LOG_LEN        (2000)          //2000 x 1 ms = 2 s
volatile uint8_t  log_raw[LOG_LEN];
volatile uint8_t  log_filt[LOG_LEN];
volatile uint32_t log_idx = 0;

void depth_logger_task(void *argument)
{
    (void)argument;
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        if (log_idx >= LOG_LEN) {
            vTaskSuspend(NULL);        //đủ dữ liệu: dừng, không tốn CPU
        }
        log_raw[log_idx]  = (uint8_t)uxQueueMessagesWaiting(queue_raw);
        log_filt[log_idx] = (uint8_t)uxQueueMessagesWaiting(queue_filtered);
        log_idx++;
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(LOG_PERIOD_MS));
    }
}

void app_main(void)
{
    queue_raw      = xQueueCreate(QUEUE_LENGTH, sizeof(raw_sample_t));
    queue_filtered = xQueueCreate(QUEUE_LENGTH, sizeof(filtered_sample_t));
    if (queue_raw == NULL || queue_filtered == NULL) {
        Error_Handler();
    }

    //So sánh từng lời gọi với pdPASS
    //(errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY = -1) AND với pdPASS (1) vẫn ra 1.
    if (xTaskCreate(sensor_task,       "sensor", 256, NULL, PRIO_SENSOR, NULL) != pdPASS ||
        xTaskCreate(filter_task,       "filter", 256, NULL, PRIO_FILTER, NULL) != pdPASS ||
        xTaskCreate(uart_task,         "uart",   384, NULL, PRIO_UART,   NULL) != pdPASS ||
        xTaskCreate(depth_logger_task, "logger", 128, NULL, PRIO_LOGGER, NULL) != pdPASS) {
        Error_Handler();
    }

    vTaskStartScheduler();

    for (;;) {
    }
}