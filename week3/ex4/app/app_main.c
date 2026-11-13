/* Week 3 / Ex4 -- CPU load, deadlock, watchdog.
 * Native FreeRTOS API only.
 */
#include "FreeRTOS.h"
#include "task.h"

#include "main.h"
#include "params.h"
#include "runtime_stats.h"
#include "deadlock_demo.h"
#include "supervisor.h"
#include <stdio.h>

void app_main(void)
{
    printf("boot\r\n");

    runtime_stats_timer_init(); //bật đồng hồ DWT trước khi scheduler bắt đầu đo
    deadlock_demo_init(); //tạo hai mutex, vì task dùng ngay khi chạy.
    supervisor_init(); //tạo event group và bật IWDG

    //khởi tạo 4 task
    xTaskCreate(task_alpha, "task_alpha", 128, NULL, 2, NULL);
    xTaskCreate(task_beta, "task_beta", 128, NULL, 2, NULL);
    xTaskCreate(supervisor_task, "supervisor", 128, NULL, 3, NULL); //ưu tiên cao nhất
    xTaskCreate(runtime_stats_task, "stats", 512, NULL, 1, NULL); //cần nhiều stack hơn do printf và hàm vTaskGetRunTimeStats

    vTaskStartScheduler();

    for (;;) {
    }
}
