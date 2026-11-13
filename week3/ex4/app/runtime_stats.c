/* Week 3 / Ex4 -- run-time stats on DWT CYCCNT.
 * Native FreeRTOS API only.
 */
#include <stdint.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

#include "main.h"
#include "params.h"
#include "runtime_stats.h"
#include "supervisor.h" //để quản lý task cho iwdg

#define SUPERVISOR_BIT_STATS   (1u << 2) //task này dùng bit2 trong eventgroup, bit0 cho alpha, bit1 cho beta
#define STATS_MAX_TASKS        (8)

void runtime_stats_timer_init(void)
{
    //init dwt
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

//gọi mỗi lần chuyển task
uint32_t runtime_stats_timer_value(void)
{
    return DWT->CYCCNT;
}

void runtime_stats_task(void *argument)
{
    (void)argument;
    static char buf[512];

    static TaskStatus_t status[STATS_MAX_TASKS];//mảng trạng thái của task

    static const char *const state_name[] = {
        "eRunning", "eReady", "eBlocked", "eSuspended", "eDeleted", "eInvalid"
    };
    uint32_t total_time;
    UBaseType_t n;
    UBaseType_t i;
 
    for (;;) {
        //hàm ghi vào buf một bảng chữ, mỗi dòng gồm tên task, 
        //thời gian tuyệt đối (số nhịp) và phần trăm, phần trăm này tính từ lúc boot đến giờ
        vTaskGetRunTimeStats(buf);
        printf("%s\r\n", buf);
        //hàm điền vào mỗi ô của status thông tin của một task (tên, trạng thái, mức ưu tiên, bộ đếm run-time...) và trả về số task
        n = uxTaskGetSystemState(status, STATS_MAX_TASKS, &total_time);
        //in mỗi task một dòng gồm tên (rộng 12 ký tự, canh trái), trạng thái, và bộ đếm run-time
        for (i = 0; i < n; i++) {
            printf("%-12s %-10s runtime=%lu\r\n",
                   status[i].pcTaskName,
                   state_name[status[i].eCurrentState],
                   (unsigned long)status[i].ulRunTimeCounter);
        }
        printf("\r\n");
 
        //IWDG chỉ chờ 250 ms, nhưng task này lại delay tận 2 giây -> chia nhỏ thành các lần ngủ 50ms ->
        for (i = 0; i < (STATS_WINDOW_MS / 50); i++) {
            supervisor_checkin(SUPERVISOR_BIT_STATS); //bật bit2 trong eventgroup rồi ngủ 50ms
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}
