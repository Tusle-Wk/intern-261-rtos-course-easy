/* Week 2 / Ex3 — priority inversion vs priority inheritance.
 *
 * task_low  (lowest prio)  holds the shared resource for LOW_HOLD_MS.
 * task_mid  (middle prio)  never touches the resource; pure CPU-bound work,
 *                          its only job is to be ready and preemptible so it
 *                          can steal CPU from task_low while task_low holds
 *                          the resource task_high is waiting for.
 * task_high (highest prio) needs the resource every HIGH_PERIOD_MS and is
 *                          the one whose block time we measure.
 */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "main.h"
#include "params.h"
#include "tasks_lmh.h"

/* ---- Logic analyzer pins (4 channels). Change to match your wiring. ----
 *  LA_LOW_*   : high while task_low holds the resource
 *  LA_MID_*   : high while task_mid burns CPU
 *  LA_HWANT_* : high from "task_high wants resource" until it gives it back
 *  LA_HGOT_*  : high from "task_high got resource" until it gives it back
 *  Block time t1 - t0 = rising edge of LA_HGOT  -  rising edge of LA_HWANT. */
#ifndef LA_LOW_PORT
#define LA_LOW_PORT    GPIOA
#define LA_LOW_PIN     GPIO_PIN_0
#define LA_MID_PORT    GPIOA
#define LA_MID_PIN     GPIO_PIN_1
#define LA_HWANT_PORT  GPIOA
#define LA_HWANT_PIN   GPIO_PIN_2
#define LA_HGOT_PORT   GPIOA
#define LA_HGOT_PIN    GPIO_PIN_3
#endif

#define LA_HIGH(p)  HAL_GPIO_WritePin(p##_PORT, p##_PIN, GPIO_PIN_SET)
#define LA_LOW(p)   HAL_GPIO_WritePin(p##_PORT, p##_PIN, GPIO_PIN_RESET)

#define LOW_IDLE_MS   (1)   /* task_low pause after giving the resource   */
#define MID_IDLE_MS   (5)   /* task_mid pause so it keeps becoming Ready  */
#define HIGH_WORK_MS  (1)   /* small work task_high does inside the resource */

static SemaphoreHandle_t s_resource;

/* Observable from the debugger / watch window (cross-check the LA photo). */
volatile uint32_t g_high_block_us;      /* last measured block of task_high   */
volatile uint32_t g_high_miss_count;    /* times block > DEADLINE_MS           */
volatile UBaseType_t g_low_prio_max;    /* highest prio task_low saw in hold   */

/* CPU-time busy wait using the DWT cycle counter.
 * Only time actually spent RUNNING counts: if the task is preempted, the gap
 * between two consecutive samples is large and is NOT added. This matters —
 * task_low must need LOW_HOLD_MS of real CPU, otherwise a wall-clock wait
 * would "expire" while task_mid is running and hide part of the inversion. */
static void burn_ms(uint32_t ms)
{
    const uint32_t cyc_per_ms = SystemCoreClock / 1000U;
    const uint32_t gap_limit  = SystemCoreClock / 10000U;  /* 100 us */
    uint32_t target = ms * cyc_per_ms;
    uint32_t done   = 0;
    uint32_t prev   = DWT->CYCCNT;

    while (done < target) {
        uint32_t now = DWT->CYCCNT;
        uint32_t d   = now - prev;
        prev = now;
        if (d < gap_limit) {
            done += d;
        }
    }
}

void resource_init(void)
{
    /* LA GPIOs as push-pull outputs (skip if CubeMX already did it). */
    GPIO_InitTypeDef gi = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    gi.Mode  = GPIO_MODE_OUTPUT_PP;
    gi.Pull  = GPIO_NOPULL;
    gi.Speed = GPIO_SPEED_FREQ_HIGH;
    gi.Pin   = LA_LOW_PIN | LA_MID_PIN | LA_HWANT_PIN | LA_HGOT_PIN;
    HAL_GPIO_Init(GPIOA, &gi);
    LA_LOW(LA_LOW); LA_LOW(LA_MID); LA_LOW(LA_HWANT); LA_LOW(LA_HGOT);

    /* DWT cycle counter (same as ex2 helper). */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;

#if RESOURCE_MODE == 0
    /* TODO 1: xSemaphoreCreateBinary(), then give it once so it starts
     *         "available" (binary semaphores start empty). */

    //tạo semaphore
    s_resource = xSemaphoreCreateBinary();
    if (s_resource == NULL) {
        for (;;) {}
    }
    xSemaphoreGive(s_resource);
#else
    /* TODO 2: xSemaphoreCreateMutex(). */
    s_resource = xSemaphoreCreateMutex();
    if (s_resource == NULL) {
        for (;;) {}
    }
#endif
}

void task_low(void *argument)
{
    (void)argument;
    for (;;) {
        /* TODO 3: take s_resource (portMAX_DELAY), toggle a GPIO marking
         *         "resource held", busy-wait LOW_HOLD_MS using the ex2 DWT
         *         helper or a HAL tick-based wait, lower the GPIO, give
         *         s_resource back. Then yield/delay briefly before looping —
         *         this task must not monopolize the CPU outside the hold. */
        //chờ vô hạn cho đến khi lấy được tài nguyên, rồi kéo kênh PA0 lên mức cao.
        xSemaphoreTake(s_resource, portMAX_DELAY);
        LA_HIGH(LA_LOW);

        g_low_prio_max = 0;
        for (uint32_t i = 0; i < LOW_HOLD_MS; i++) { //giữ tài nguyên 12 ms, chia thành 12 lát 1 ms
            burn_ms(1);
            UBaseType_t p = uxTaskPriorityGet(NULL);//Sau mỗi lát hỏi ưu tiên hiện tại của chính mình (NULL nghĩa là task đang gọi) và lưu giá trị lớn nhất.
            if (p > g_low_prio_max) {
                g_low_prio_max = p;
            }
        }
        //hạ kênh PA0, nhả tài nguyên, nghỉ 1 ms rồi lặp lại.
        LA_LOW(LA_LOW);
        xSemaphoreGive(s_resource);

        vTaskDelay(pdMS_TO_TICKS(LOW_IDLE_MS));
    }
}

void task_mid(void *argument)
{
    (void)argument;
    for (;;) {
        /* TODO 4: toggle a GPIO, busy-wait MID_WORK_MS, lower the GPIO, then
         *         vTaskDelay a short period so it keeps becoming Ready
         *         periodically rather than running once and blocking
         *         forever. This task never touches s_resource. */
        //không động đến s_resource, chỉ chiếm CPU 30 ms, nghỉ 5 ms rồi lặp lại.
        LA_HIGH(LA_MID);
        burn_ms(MID_WORK_MS);
        LA_LOW(LA_MID);

        vTaskDelay(pdMS_TO_TICKS(MID_IDLE_MS));
    }
}

void task_high(void *argument)
{
    (void)argument;
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        /* TODO 5: toggle a GPIO high (marks "task_high wants the resource"),
         *         take s_resource (portMAX_DELAY), toggle a second GPIO
         *         transition (marks "task_high got it"), do a small amount
         *         of work, give s_resource back, lower the first GPIO.
         *         The gap between the two GPIO edges on the logic analyzer
         *         is your block time t1 - t0. */
        //ghi mốc t0, kéo PA2 lên, rồi chờ tài nguyên
        uint32_t t0 = DWT->CYCCNT;
        
        LA_HIGH(LA_HWANT);                        

        xSemaphoreTake(s_resource, portMAX_DELAY);
        //khi lấy được thì kéo PA3 lên và ghi t1
        LA_HIGH(LA_HGOT);                         
        uint32_t t1 = DWT->CYCCNT;

        g_high_block_us = (uint32_t)(((uint64_t)(t1 - t0) * 1000000ULL) / SystemCoreClock);
        if (g_high_block_us > (uint32_t)DEADLINE_MS * 1000U) {
            g_high_miss_count++;
        }
        //làm việc 1 ms trong vùng giữ tài nguyên, nhả tài nguyên, hạ hai kênh.
        burn_ms(HIGH_WORK_MS);

        xSemaphoreGive(s_resource);
        LA_LOW(LA_HGOT);
        LA_LOW(LA_HWANT);

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(HIGH_PERIOD_MS));
    }
}