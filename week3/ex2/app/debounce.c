/* Week 3 / Ex2 -- debounce via one-shot timer + long-press via auto-reload
 * timer. Native FreeRTOS API only.
 */
#include <stdio.h>

#include "FreeRTOS.h"
#include "timers.h"

#include "main.h"
#include "params.h"
#include "debounce.h"

#define PROBE_PORT GPIOA
#define PROBE_PIN  GPIO_PIN_1   /* PA1: tín hiệu sau debounce, cao = đang nhấn */

static volatile uint32_t   s_edges;           /* tổng số cạnh, kể cả nảy */
static volatile TickType_t s_last_edge_tick;  /* tick của cạnh gần nhất */

//nhấn pull up -> mặc định là 1, nhấn là 0
#ifndef BTN_PRESSED_LEVEL
#define BTN_PRESSED_LEVEL 0
#endif
 
/* Ten timer phai vua TIMER_NAME_LEN (bytes, ke ca ky tu '\0'). */
#define DEBOUNCE_TIMER_NAME  "debounce"
#define LONGPRESS_TIMER_NAME "longpress"
_Static_assert(sizeof(DEBOUNCE_TIMER_NAME)  <= TIMER_NAME_LEN, "debounce timer name too long"); //kiểm tra độ dài tên có lố 12 bytes không(tính cả \0)
_Static_assert(sizeof(LONGPRESS_TIMER_NAME) <= TIMER_NAME_LEN, "longpress timer name too long");
//handle do xTimerCreate trả về
static TimerHandle_t s_debounce_timer;
static TimerHandle_t s_longpress_timer;

static volatile TickType_t s_press_tick;//lưu tick của cạnh xuống gần nhất, dùng để tính thời gian giữ thực tế.
//phân biệt PRESS va RELEASE.
static volatile uint8_t s_pressed;
//hàm chạy trong daemon task
static void debounce_callback(TimerHandle_t xTimer)
{
    (void)xTimer;
    //test
    static uint32_t edges_reported; //nhớ số cạnh đã báo lần trước
    uint32_t   edges = s_edges; //tổng số cạnh ISR đã đếm đến lúc này
    TickType_t now   = xTaskGetTickCount();

    HAL_GPIO_WritePin(PROBE_PORT, PROBE_PIN, s_pressed ? GPIO_PIN_SET : GPIO_PIN_RESET);

    printf("%s t=%lu edges=%lu since_last_edge=%lu ms\r\n",
           s_pressed ? "PRESS" : "RELEASE", //in ra PRESS hay RELEASE
           (unsigned long)now, //in ra tick lúc callback chạy
           (unsigned long)(edges - edges_reported), //số cạnh trong chùm vừa rồi, =1 thì clean, >1 thì là nảy
           (unsigned long)((now - s_last_edge_tick) * portTICK_PERIOD_MS)); //khoảng cách từ cạnh cuối đến lúc callback chạy, kỳ vọng khoảng 20 ms
    edges_reported = edges;

    /* TODO 1: printf/HAL_UART "PRESS" + xTaskGetTickCount(). This is the
     *         stable-press event, DEBOUNCE_MS after the last edge.
     */ 
    /* TODO 2 (temporary, for the README step-4 experiment ONLY): add
     *         vTaskDelay(pdMS_TO_TICKS(50)) here, capture what happens to
     *         longpress_timer's callback timing in your REPORT.md, then
     *         DELETE this line again before you submit. A callback that
     *         blocks here is blocking the ONE daemon task every other
     *         software timer in the system also depends on. */

}

//miễn là còn nhấn giữ thì cứ 1 giây sẽ gọi hàm này 1 lần
static void longpress_callback(TimerHandle_t xTimer)
{
    (void)xTimer;

    /* TODO 3: printf/HAL_UART "HELD" + xTaskGetTickCount(). Fires once per
     *         LONGPRESS_MS while the button stays down. */
    //in thêm thời gian giữ thực tế
    TickType_t now = xTaskGetTickCount();
    printf("HELD t=%lu held=%lu ms\r\n",
           (unsigned long)now,
           (unsigned long)((now - s_press_tick) * portTICK_PERIOD_MS));//số tick đã giữ x thòi gian 1 tick = thời gian đã giữ
}

void debounce_init(void)
{
    /* TODO 4: s_debounce_timer = xTimerCreate(..., pdMS_TO_TICKS(DEBOUNCE_MS),
     *         pdFALSE, NULL, debounce_callback); -- pdFALSE = one-shot.
     *         s_longpress_timer = xTimerCreate(..., pdMS_TO_TICKS(LONGPRESS_MS),
     *         pdTRUE, NULL, longpress_callback); -- pdTRUE = auto-reload.
     *         Check both handles non-NULL. Do NOT xTimerStart() either here --
     *         they are driven entirely from debounce_on_edge(). */
    s_debounce_timer = xTimerCreate(DEBOUNCE_TIMER_NAME, //tên debug
                                    pdMS_TO_TICKS(DEBOUNCE_MS), //chu kỳ tính bằng tick
                                    pdFALSE,            //one-shot
                                    NULL,
                                    debounce_callback); //callback của timer này
    s_longpress_timer = xTimerCreate(LONGPRESS_TIMER_NAME,
                                     pdMS_TO_TICKS(LONGPRESS_MS),
                                     pdTRUE,            // auto-reload
                                     NULL,
                                     longpress_callback);
 
    configASSERT(s_debounce_timer != NULL); //check đủ heap để tạo timer không
    configASSERT(s_longpress_timer != NULL);

    if ((s_debounce_timer == NULL) || (s_longpress_timer == NULL)) {
        for (;;) {
        }
    }
}

void debounce_on_edge(uint8_t pin_is_high)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;// cờ khi một task có ưu tiên cao hơn task đang bị ngắt được đánh thức

    uint8_t pressed = (pin_is_high == BTN_PRESSED_LEVEL); //cờ lưu trạng thái nhấn
    s_pressed = pressed;

    s_edges++; //test
    s_last_edge_tick = xTaskGetTickCountFromISR();//test

    /* TODO 5: xTimerResetFromISR(s_debounce_timer, &xHigherPriorityTaskWoken)
     *         on every edge -- this is what swallows bounce: each new edge
     *         restarts the DEBOUNCE_MS countdown, so the callback only fires
     *         once the line has been quiet for DEBOUNCE_MS. */
     xTimerResetFromISR(s_debounce_timer, &xHigherPriorityTaskWoken);
    /* TODO 6: if (pin_is_high) xTimerStartFromISR(s_longpress_timer, ...);
     *         else xTimerStopFromISR(s_longpress_timer, ...); */
    if (pressed) {
        s_press_tick = xTaskGetTickCountFromISR();
        xTimerStartFromISR(s_longpress_timer, &xHigherPriorityTaskWoken);
    } else {
        xTimerStopFromISR(s_longpress_timer, &xHigherPriorityTaskWoken);
    }
    /* TODO 7: portYIELD_FROM_ISR(xHigherPriorityTaskWoken). */
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    
}
