#ifndef SAMPLER_ISR_H
#define SAMPLER_ISR_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

extern QueueHandle_t sample_queue;
extern TaskHandle_t sampler_task_handle; //thêm handle của task lấy mẫu extern

/* Starts a hardware timer (TIMx, not TIM4 -- that one is the HAL timebase)
 * that interrupts at SAMPLE_RATE_HZ and calls sampler_timer_callback() from
 * its ISR. */
void sampler_isr_init(void);

/* Called from the timer ISR. Reads one sample and pushes it to
 * sample_queue. Runs in interrupt context -- every FreeRTOS call made here
 * MUST be the ...FromISR variant. */
void sampler_timer_callback(void);

#endif /* SAMPLER_ISR_H */
