/* Week 2 / Ex4 — event group & task notification.
 * Native FreeRTOS API only.
 */

#include "FreeRTOS.h"
#include "task.h"

#include "main.h"
#include "params.h"
#include "sync_tasks.h"

/* Thiết kế priority (cần configMAX_PRIORITIES >= 3):
 *   sync_task  >  producer  >  idle
 * sync_task cao nhất để nó chiếm CPU ngay khi được producer đánh thức. Nhờ đó
 * khoảng "producer gọi set/notify -> sync_task chạy" đo đúng đường đi đánh thức
 * trong kernel, không bị lẫn chính sách lập lịch. */
#define PRODUCER_PRIO        (tskIDLE_PRIORITY + 1)
#define SYNC_TASK_PRIO       (tskIDLE_PRIORITY + 2)

/* Đơn vị là word
 * sync_task gọi printf nên cần stack lớn hơn nhiều so với producer.
 * Kiểm tra bằng uxTaskGetStackHighWaterMark() và giảm nếu thiếu RAM. */
#define SYNC_TASK_STACK_WORDS  512
#define PRODUCER_STACK_WORDS   128

void app_main(void)
{
    sync_init();   //tạo event group + bật DWT trước khi có task nào chạy 

    /* tạo sync_task trước để sync_task_handle đã hợp lệ trước khi
     * bất kỳ producer nào chạy (producer_0 cần handle khi dùng notify). */
    TaskHandle_t h_sync = NULL;
    if (xTaskCreate(sync_task, "sync", SYNC_TASK_STACK_WORDS, NULL,
                    SYNC_TASK_PRIO, &h_sync) != pdPASS) {
        for (;;) {                 //hết heap: dừng tại đây
        }
    }
    sync_task_handle = (void *)h_sync;   //lưu handle dạng void* theo header

    //tạo EVENT_BIT_COUNT producer, tham số task = chỉ số 0..N-1. 
    for (uint32_t i = 0; i < EVENT_BIT_COUNT; i++) {
        char name[8] = "prod_X";
        name[5] = (char)('0' + i);   //prod_0, prod_1, prod_2 (kernel sao chép tên vào TCB nên mảng trên stack là an toàn)

        if (xTaskCreate(producer_task, name, PRODUCER_STACK_WORDS,
                        (void *)(uintptr_t)i, PRODUCER_PRIO, NULL) != pdPASS) {
            for (;;) {
            }
        }
    }

    vTaskStartScheduler();  //không bao giờ trở về nếu đủ heap cho idle task

    for (;;) {
    }
}