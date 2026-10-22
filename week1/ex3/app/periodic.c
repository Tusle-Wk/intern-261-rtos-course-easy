/* Week 1 / Ex3 — periodic task: vTaskDelay vs xTaskDelayUntil.
 *
 * Build this file twice over the exercise, switching DELAY_MODE, and twice more
 * with configTICK_RATE_HZ changed in FreeRTOSConfig.h. Four runs total.
 */
#include "FreeRTOS.h"
#include "task.h"

#include "main.h"
#include "params.h"

/* 0 = vTaskDelay, 1 = xTaskDelayUntil */
#ifndef DELAY_MODE
#define DELAY_MODE 1
#endif

/* Chu ky phai >= 1 tick, neu khong vTaskDelay(0) chi yield, khong con la tuan hoan. */
_Static_assert(pdMS_TO_TICKS(PERIOD_MS) > 0,
               "PERIOD_MS nho hon 1 tick o configTICK_RATE_HZ hien tai");

/* Fixed seed: the variable load must be reproducible across all four runs,
 * otherwise the four configurations are not comparable. */
static uint32_t s_rand_state = 0x12345678u;

/* Bien quan sat bang debugger / SWV (phuc vu muc 4 cua REPORT.md). */
volatile uint32_t g_cycle_count     = 0;  /* so chu ky da chay */
volatile uint32_t g_missed_deadline = 0;  /* so lan xTaskDelayUntil tra ve pdFALSE */

static uint32_t next_work_us(void)
{
    /* xorshift32 — state khac 0 nen khong bao gio ket o 0. */
    uint32_t x = s_rand_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s_rand_state = x;

    /* Map vao [0, WORK_JITTER_US] (bao gom ca 2 dau). */
    return x % (WORK_JITTER_US + 1u);
}

/* Busy-wait bang DWT->CYCCNT (dwt_init() da bat CYCCNT). Xu ly tran 32-bit
 * bang phep tru unsigned. Neu ex2 cua ban da co ham tuong duong thi co the
 * thay bang ham do. */
static void burn_us(uint32_t us)
{
    const uint32_t cycles_per_us = SystemCoreClock / 1000000u;
    const uint32_t start  = DWT->CYCCNT;
    const uint32_t target = us * cycles_per_us;

    while ((uint32_t)(DWT->CYCCNT - start) < target) {
        /* spin */
    }
}

void periodic_task(void *argument)
{
    (void)argument;

    const TickType_t period_ticks = pdMS_TO_TICKS(PERIOD_MS);  /* khong hard-code so tick */

#if DELAY_MODE == 1
    /* Khoi tao NGAY TRUOC vong lap, ben trong task (khong phai global, khong
     * phai trong vong lap). Chi xTaskDelayUntil moi duoc cap nhat bien nay. */
    TickType_t last_wake_time = xTaskGetTickCount();
#endif

    for (;;) {
        /* Toggle dau chu ky (truoc tai gia) de canh logic analyzer phan anh
         * thoi diem task thuc day, khong bi cong them do tai. */
        GPIOA->ODR ^= (1u << PERIODIC_PIN);

        burn_us(next_work_us());

        g_cycle_count++;

#if DELAY_MODE == 0
        /* Tuong doi: tinh tu LUC GOI (tuc sau khi tai chay xong) -> chu ky thuc
         * = period + thoi gian tai + lam tron tick => drift cong don. */
        vTaskDelay(period_ticks);
#else
        /* Tuyet doi: thuc day tai last_wake_time + period_ticks, bat ke tai chay
         * bao lau. Tra ve pdTRUE neu task thuc su bi block (dung han),
         * pdFALSE neu deadline da qua truoc khi goi (tre -> khong block). */
        BaseType_t was_delayed = xTaskDelayUntil(&last_wake_time, period_ticks);
        if (was_delayed == pdFALSE) {
            g_missed_deadline++;
        }
#endif
    }
}