/* Week 3 / Ex4 -- supervisor task: waits for all monitored tasks to check in
 * via an event group every cycle; refreshes IWDG only if they all did.
 * Native FreeRTOS API only.
 */
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"

#include "main.h"
#include "params.h"
#include "supervisor.h"

#include <stdio.h>
EventGroupHandle_t supervisor_events; //handle của event group

#define ALL_BITS ((1u << SUPERVISOR_BITS) - 1u) // =7 = 0b111

#define IWDG_LSI_HZ        (40000u) //iwdg dùng lsi, cỡ 40khz
#define IWDG_PRESCALER_DIV (4u)     //prescaler /4 cho iwdg
#define IWDG_RELOAD        ((IWDG_TIMEOUT_MS * (IWDG_LSI_HZ / IWDG_PRESCALER_DIV)) / 1000u) //40khz/4=10k nhịp mỗi giây, cần 250ms ->2500 nhịp
 
#define SUPERVISOR_WAIT_MS (100) //thời gian tối đa mỗi lần supervisor chờ đủ bit, phải nhỏ hơn tg reset của iwdg
 
static IWDG_HandleTypeDef hiwdg; //handle của iwdg

void supervisor_init(void)
{
    supervisor_events = xEventGroupCreate(); //tạo eventgroup
    configASSERT(supervisor_events != NULL);//check ram đủ để tạo hay không
 
    //config cho iwdg
    hiwdg.Instance = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_4;
    hiwdg.Init.Reload = IWDG_RELOAD;
    //lỗi khởi tạo thì vô loop
    if (HAL_IWDG_Init(&hiwdg) != HAL_OK) {
        for (;;) {
        }
    }
}

void supervisor_task(void *argument)
{
    (void)argument;
    EventBits_t bits;
 
    for (;;) {
        //chờ đủ các task bật bit 
        bits = xEventGroupWaitBits(supervisor_events, ALL_BITS, pdTRUE,
                                   pdTRUE, pdMS_TO_TICKS(SUPERVISOR_WAIT_MS));
 
        if ((bits & ALL_BITS) == ALL_BITS) { //tất cả bật bit hết rồi thì mới nạp lại iwdg
            HAL_IWDG_Refresh(&hiwdg);
        }
    }

    
}
