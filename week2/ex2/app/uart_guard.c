/* Week 2 / Ex2 — four ways to serialize access to USART1 from WRITER_COUNT
 * tasks. Only one UART_GUARD_MODE is compiled at a time.
 */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include <string.h>

#include "main.h"
#include "params.h"
#include "uart_guard.h"

extern UART_HandleTypeDef huart1;

#define UART_TX_TIMEOUT_MS      (100)       /* timeout của HAL_UART_Transmit */
#define UART_SPIN_LIMIT         (200000U)   /* giới hạn vòng chờ cờ khi ghi thẳng thanh ghi */

/* Mode 0: 0 = ghi thẳng thanh ghi USART1 từng byte (lỗi lẫn ký tự tái hiện ổn định)
 *         1 = gọi HAL_UART_Transmit trần (HAL trả HAL_BUSY nếu UART đang bận =>
 *             bản tin bị MẤT chứ không lẫn; đếm vào g_busy_count). */
#ifndef UART_MODE0_USE_HAL
#define UART_MODE0_USE_HAL      (0)
#endif

//Số đo cho REPORT: đọc bằng Watch khi chip đang Pause
volatile uint32_t g_lat_count  = 0;   //số bản tin đã đo
volatile uint64_t g_lat_sum_us = 0;   //tổng độ trễ (us); trung bình = sum / count
volatile uint32_t g_lat_max_us = 0;   //độ trễ lớn nhất (us)
volatile uint32_t g_tx_errors  = 0;   //HAL_UART_Transmit trả về khác HAL_OK
volatile uint32_t g_busy_count = 0;   //Mode 0 + HAL: số lần bị HAL_BUSY
volatile uint32_t g_cs_last_us = 0;   //Mode 2: thời gian giữ critical section lần gần nhất (us)
volatile uint32_t g_cs_max_us  = 0;   //Mode 2: lớn nhất

//Độ trễ in = từ lúc writer gọi uart_guard_send() ("muốn in") đến lúc in xong
//Đồng hồ: bộ đếm chu kỳ DWT->CYCCNT (72 MHz => ~14 ns/tick)
static void lat_record(uint32_t cycles)
{
    const uint32_t us = cycles / (SystemCoreClock / 1000000U);

    //Nhiều writer cùng cập nhật số đo => bảo vệ bằng đoạn critical RẤT ngắn
    //(vài chu kỳ, << 1 us), không đáng kể so với ~1740 us truyền 1 bản tin
    taskENTER_CRITICAL();
    g_lat_count++;
    g_lat_sum_us += us;
    if (us > g_lat_max_us)
        g_lat_max_us = us;
    
    taskEXIT_CRITICAL();
}

//Gửi bằng HAL (polling). Dùng cho Mode 1, 2, 3
static inline void uart_tx_hal(const char *buf, size_t len)
{
    if (HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)len, UART_TX_TIMEOUT_MS) != HAL_OK) {
        g_tx_errors++;
    }
}

#if UART_GUARD_MODE == 0
// bug: không có tuần tự nào
static void uart_tx_noguard(const char *buf, size_t len)
{
#if UART_MODE0_USE_HAL
    HAL_StatusTypeDef st = HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)len, UART_TX_TIMEOUT_MS);

    if (st == HAL_BUSY) {
        g_busy_count++;
    } else if (st != HAL_OK) {
        g_tx_errors++;
    }
#else
    /* Ghi thẳng từng byte vào thanh ghi dữ liệu: không có cờ trạng thái nào của HAL
     * chặn writer thứ hai, nên khi scheduler đổi task giữa chừng, byte của hai
     * writer xen kẽ nhau trên đường truyền. */
    USART_TypeDef *u = huart1.Instance;
    uint32_t spin;

    for (size_t i = 0; i < len; i++) {
        spin = UART_SPIN_LIMIT;
        while (((u->SR & USART_SR_TXE) == 0U) && (--spin != 0U)) {} //chờ DR trống
        u->DR = (uint8_t)buf[i];    //đẩy 1 byte
    }
    spin = UART_SPIN_LIMIT;
    while (((u->SR & USART_SR_TC) == 0U) && (--spin != 0U)) {} //chờ byte cuối ra khỏi dây
#endif
}
#endif

#if UART_GUARD_MODE == 1
static SemaphoreHandle_t s_uart_mutex;
#elif UART_GUARD_MODE == 3
/* Phần tử queue = bản tin MSG_LEN byte + mốc thời gian lúc writer muốn in
 * (để gatekeeper đo được độ trễ "đến lúc in xong"). */
typedef struct {
    char     data[MSG_LEN];
    uint32_t t_req; //mốc lúc writer muốn in, để gatekeeper đo độ trễ
} guard_msg_t;

//độ dài queue trong tình huống xấu nhất cả WRITER_COUNT writer gửi cùng một
//thời điểm => cần ít nhất WRITER_COUNT chỗ, nhân 2 để dự phòng một chu kỳ trễ

#define GATEKEEPER_QUEUE_LEN    (WRITER_COUNT * 2) //=6
#define GATEKEEPER_STACK_WORDS  (256)

static QueueHandle_t s_gatekeeper_queue;
#endif
//bật bộ đếm DWT
void uart_guard_init(void)
{
    // Bật bộ đếm chu kỳ DWT dùng để đo độ trễ
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
//mode 0 và 2 không tạo thêm gì nên RAM thêm là 0
#if UART_GUARD_MODE == 1 //mode 1 thì tạo mutex
    s_uart_mutex = xSemaphoreCreateMutex();
    if (s_uart_mutex == NULL) {
        Error_Handler();
    }
#elif UART_GUARD_MODE == 3 //mode 3 tạo queue và task gatekeeper
    s_gatekeeper_queue = xQueueCreate(GATEKEEPER_QUEUE_LEN, sizeof(guard_msg_t));
    if (s_gatekeeper_queue == NULL) {
        Error_Handler();
    }
    if (xTaskCreate(gatekeeper_task, "gatekp", GATEKEEPER_STACK_WORDS, NULL, GATEKEEPER_PRIO, NULL) != pdPASS) {
        Error_Handler();
    }
#endif
}

void uart_guard_send(const char *buf, size_t len)
{
#if UART_GUARD_MODE == 3
    //Writer không bao giờ chạm UART, chỉ bỏ bản tin vào queue
    guard_msg_t m;

    configASSERT(len == MSG_LEN);
    memcpy(m.data, buf, MSG_LEN);
    m.t_req = DWT->CYCCNT;
    if (xQueueSend(s_gatekeeper_queue, &m, portMAX_DELAY) != pdPASS) {
        g_tx_errors++;
    }
    //Độ trễ của Mode 3 được ghi ở gatekeeper_task, sau khi in xong

#else
    const uint32_t t0 = DWT->CYCCNT;

#if UART_GUARD_MODE == 0
    uart_tx_noguard(buf, len);

#elif UART_GUARD_MODE == 1
    //chỉ một điểm thoát giữa Take và Give: Give luôn được gọi nếu Take thành công.
    if (xSemaphoreTake(s_uart_mutex, portMAX_DELAY) == pdTRUE) {
        uart_tx_hal(buf, len);
        xSemaphoreGive(s_uart_mutex);
    }

#elif UART_GUARD_MODE == 2
    taskENTER_CRITICAL();
    {
        const uint32_t c0 = DWT->CYCCNT;
        uart_tx_hal(buf, len);
        const uint32_t us = (DWT->CYCCNT - c0) / (SystemCoreClock / 1000000U); //thời gian che ngắt tính theo us
        g_cs_last_us = us;
        if (us > g_cs_max_us) {
            g_cs_max_us = us;
        }
    }
    taskEXIT_CRITICAL();
#endif

    lat_record(DWT->CYCCNT - t0);
#endif
}

#if UART_GUARD_MODE == 3
void gatekeeper_task(void *argument)
{
    (void)argument;
    for (;;) {
        guard_msg_t m;
        if (xQueueReceive(s_gatekeeper_queue, &m, portMAX_DELAY) == pdPASS) {
            //task duy nhất được gọi HAL_UART_Transmit trong mode này
            uart_tx_hal(m.data, MSG_LEN);
            lat_record(DWT->CYCCNT - m.t_req);
        }
    }
}
#endif