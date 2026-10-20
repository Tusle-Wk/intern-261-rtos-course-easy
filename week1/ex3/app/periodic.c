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
void dwt_busy_wait_us(uint32_t microseconds); //dung lai ham cua ex2
#ifndef DELAY_MODE
#define DELAY_MODE 0
#endif

/* Fixed seed: the variable load must be reproducible across all four runs,
 * otherwise the four configurations are not comparable. */
static uint32_t s_rand_state = 0x12345678u;

//biến đếm số chu kỳ và số lần trễ deadline
volatile uint32_t g_cycle_count = 0;
volatile uint32_t g_missed_deadlines = 0; //for debug, so lan tre han

static uint32_t next_work_us(void)
{
    /* TODO 1: xorshift32 on s_rand_state, mapped into [0, WORK_JITTER_US].
     *         Must give the identical sequence on every run. */
    //thuat toan xorshift32
    uint32_t x = s_rand_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s_rand_state = x;
    return x % (WORK_JITTER_US + 1u);//ep trong khoang co dinh
}


void periodic_task(void *argument)
{
    const uint16_t periodic_pin_mask = (uint16_t)(1u << PERIODIC_PIN);
    const TickType_t xPeriodTicks = pdMS_TO_TICKS(PERIOD_MS);
#if DELAY_MODE == 1
    /* TODO 2: declare and initialise the previous-wake-time variable.
     *         Initialising it in the wrong place is the classic mistake here —
     *         if your drift is not ~0, look at this first and say so in REPORT.md. */
    TickType_t xLastWakeTime = xTaskGetTickCount();
#endif

    for (;;) {
        /* TODO 3: toggle PA<PERIODIC_PIN>, then burn next_work_us() of CPU
         *         with the DWT busy-wait from ex2. */
        HAL_GPIO_TogglePin(GPIOA, periodic_pin_mask); //toggle PA6
        dwt_busy_wait_us(next_work_us());// fake load
#if DELAY_MODE == 0
        /* TODO 4a: vTaskDelay(...) — what exactly do you pass, and relative to what? */
        vTaskDelay(xPeriodTicks); //neu la vtaskdelay
#else
        /* TODO 4b: xTaskDelayUntil(...) — note it returns a value. What does it mean? */
        BaseType_t xWasDelayed = xTaskDelayUntil(&xLastWakeTime, xPeriodTicks);
        if (xWasDelayed == pdFALSE) { //truong hop load chay qua lau, vuot qua thoi gian xPeriod          
            if (g_cycle_count < SAMPLE_COUNT)
                g_missed_deadlines++;
            
        }
#endif
        g_cycle_count++;
    }
}
