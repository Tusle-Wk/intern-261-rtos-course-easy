/* Week 3 / Ex3 -- command parser: rx_stream -> change demo_task's
 * period/priority/suspend state at runtime.
 *
 * Commands (each terminated by '\r', max MAX_CMD_LEN bytes):
 *   P<ms>\r   -- set demo_task period to <ms>
 *   S<prio>\r -- vTaskPrioritySet(demo_task, <prio>)
 *   T<0|1>\r  -- 0 = vTaskSuspend(demo_task), 1 = vTaskResume(demo_task)
 * Moi lenh co the them hau to "#<so thu tu>" (vd "P10#42\r") de dem lenh khi test gui nhanh.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "stream_buffer.h"

#include "main.h"
#include "params.h"
#include "uart_rx.h"
#include "parser_task.h"

static TaskHandle_t s_demo_task_handle; //handle của task demo
static volatile uint32_t s_demo_period_ms = 200;//parser ghi và demo_task đọc

//đọc một số thập phân từ buffer không có ký tự NUL ở cuối, nên luôn dùng len
static int parse_uint(const char *s, size_t len, size_t *pos, uint32_t *out)
{
    size_t i = *pos;//vị trí đang đọc
    uint32_t v = 0;//giá trị tích luỹ

    while (i < len && s[i] >= '0' && s[i] <= '9') {
        if (i - *pos >= 9u) { //số chữ số đã đọc, 10 chữ số có thể vượt quá max của uint32_t, return 0 nếu vượt
            return 0;
        }
        v = v * 10u + (uint32_t)(s[i] - '0');
        i++;
    }
    //không đọc được chữ số nào thì thất bại
    if (i == *pos) {
        return 0;
    }
    *pos = i;
    *out = v;
    return 1;
}

//hàm xử lý theo lệnh đọc được
static void apply_command(const char *cmd, size_t len)
{
    size_t n = len;          //độ dài phần lệnh
    uint32_t seq = 0;       //chuỗi số đọc được
    int has_seq = 0;        //đọc được chuỗi số hay không
    uint32_t v = 0;
    size_t pos = 1;
    TaskHandle_t h = s_demo_task_handle;

    const char *hash = (const char *)memchr(cmd, '#', len); //tìm dấu # đầu tiên trong len byte
    if (hash != NULL) { //nếu có thì n thu lại chỉ còn phần lệnh
        n = (size_t)(hash - cmd);
        size_t p = n + 1u;  //số thứ tự bắt đầu sau dấu #
        if (!parse_uint(cmd, len, &p, &seq) || p != len) { //sai cú pháp hoặc p sau khi chạy hàm không bằng len-> bỏ qua
            return;          
        }
        has_seq = 1;
    }
    //dòng rỗng
    if (n == 0u) {
        return;
    }
    //check ký tự đầu chuỗi xem là lệnh loại gì
    switch (cmd[0]) {
    case 'P': {
        if (!parse_uint(cmd, n, &pos, &v) || pos != n || v == 0u) { //sau khi chạy hàm parse_uint mà pos khác n hoặc v=0 thì loại
            return;
        }
        const uint32_t old = s_demo_period_ms; //lưu giá trị cũ trước khi ghi đè giá trị mới đọc được từ lệnh
        s_demo_period_ms = v; 
        printf("[P] period %lu -> %lu ms", (unsigned long)old, (unsigned long)v);
        break;
    }
    case 'S': {
        //tương tự
        if (!parse_uint(cmd, n, &pos, &v) || pos != n
                    || v > (configMAX_PRIORITIES - 2u)      // tối đa 3, luôn thấp hơn parser (4)
                    || h == NULL) {                         // demo_task chưa chạy lần nào
            return;
        }
        const UBaseType_t before = uxTaskPriorityGet(h); //lưu prio cũ trước khi ghi đè prio mới
        vTaskPrioritySet(h, (UBaseType_t)v);
        printf("[S] prio %u -> %u", (unsigned)before, (unsigned)uxTaskPriorityGet(h));
        break;
    }
    case 'T':
        //nếu là lệnh T thì chỉ có thể là 0 hoặc 1
        if (n != 2u || (cmd[1] != '0' && cmd[1] != '1')) {
            return;
        }
        if (cmd[1] == '0') { //0 thì vTaskSuspend
            vTaskSuspend(h);
            printf("[T] suspend");
        } else {
            vTaskResume(h); //1 thì vTaskResume
            printf("[T] resume");
        }
        break;
    default: //ký tự lạ thì bỏ qua
        return;
    }
    //in #sốthứtự nếu có 
    if (has_seq) {
        printf(" #%lu", (unsigned long)seq);
    }
    printf("\r\n");
}

void parser_task(void *argument)
{
    (void)argument;

    char cmd[MAX_CMD_LEN]; //bộ đệm tạm để chứa lệnh, nằm trong stack của task
    size_t idx = 0;

    for (;;) {
        uint8_t byte;
        //lấy 1 byte, buffer rỗng thì task ngủ đến khi được đánh thức
        if (xStreamBufferReceive(rx_stream, &byte, 1, portMAX_DELAY) != 1) {
            continue;
        }
        if (byte == '\n') { continue; } //xử lý cho /r/n của serial debug
        if (byte == '\r') { //thấy \r thì xử lý lệnh đang gom rồi đặt lại idx = 0
            apply_command(cmd, idx);
            idx = 0;
        } else if (idx < MAX_CMD_LEN - 1) {
            cmd[idx++] = (char)byte;
        } else {
            //tràn thì bỏ cả lệnh đang gom (byte hiện tại cũng mất)
            idx = 0;
        }
    }
}

void demo_task(void *argument)
{
    (void)argument;
    s_demo_task_handle = xTaskGetCurrentTaskHandle();
    //toggle chân PA1 rồi delay
    for (;;) {
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_1);
        vTaskDelay(pdMS_TO_TICKS(s_demo_period_ms));
    }
}