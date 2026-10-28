/* Week 2 / Ex1 — moving-average filter, window = FILTER_WINDOW samples.
 * queue_raw -> queue_filtered.
 */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "main.h"
#include "params.h"
#include "pipeline.h"

#ifndef FILTER_SEND_TIMEOUT
#define FILTER_SEND_TIMEOUT  (0)
#endif

volatile uint32_t filter_drops    = 0;  //xQueueSend sang queue_filtered thất bại 
volatile uint32_t filter_seq_gaps = 0;  //mẫu mất do sensor_task (nhìn từ seq)

volatile uint32_t filt_peak = 0; //debug

void filter_task(void *argument)
{
    (void)argument;

    //Ring buffer khởi tạo = 0 => trong giai đoạn warm-up phần tử cũ nhất bị trừ ra là 0,
    //running sum vẫn đúng; chia cho count (số mẫu thực có) để không bị lệch về 0.
    static int16_t bx[FILTER_WINDOW], by[FILTER_WINDOW], bz[FILTER_WINDOW];
    int32_t  sx = 0, sy = 0, sz = 0;    //16 * 32767 vẫn nằm gọn trong int32
    uint32_t idx = 0, count = 0;
    uint32_t expect_seq = 0;

    for (;;) {
        raw_sample_t in;

        if (xQueueReceive(queue_raw, &in, portMAX_DELAY) != pdPASS) {
            continue;
        }

        if (in.seq != expect_seq) {
            filter_seq_gaps += (in.seq - expect_seq);
        }
        expect_seq = in.seq + 1;

        //O(1): trừ mẫu cũ nhất, cộng mẫu mới, ghi đè vị trí đó
        sx += in.ax - bx[idx];  bx[idx] = in.ax;
        sy += in.ay - by[idx];  by[idx] = in.ay;
        sz += in.az - bz[idx];  bz[idx] = in.az;
        idx = (idx + 1) % FILTER_WINDOW;
        if (count < FILTER_WINDOW) count++;

        filtered_sample_t out;
        out.seq = in.seq;
        out.ax = (int16_t)(sx / (int32_t)count);
        out.ay = (int16_t)(sy / (int32_t)count);
        out.az = (int16_t)(sz / (int32_t)count);

        if (xQueueSend(queue_filtered, &out, FILTER_SEND_TIMEOUT) != pdPASS) {
            filter_drops++;
        }
        //debug
        UBaseType_t d = uxQueueMessagesWaiting(queue_filtered);
        if (d > filt_peak) filt_peak = d;
    }
}