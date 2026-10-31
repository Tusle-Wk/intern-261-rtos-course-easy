/* Week 2 / Ex2 — WRITER_COUNT identical writer tasks. Each prints an MSG_LEN
 * byte string with its own id + sequence number, every WRITER_PERIOD_MS.
 */
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "main.h"
#include "params.h"
#include "uart_guard.h"

_Static_assert(MSG_LEN >= 16, "MSG_LEN qua ngan de chua header 'Wn #00000 ' + CRLF"); //bảo đảm đủ chỗ cho header và CRLF.

/* Một hàm writer_task duy nhất, chỉ số 0 1 2
 *
 * Định dạng bản tin (đúng MSG_LEN byte=40):
 *     "W<id> #<seq 5 chữ số> " + (ký tự điền riêng của writer) + "\r\n"
 * Ví dụ writer 1, MSG_LEN = 40:
 *     "W1 #00012 bbbbbbbbbbbbbbbbbbbbbbbbbbbb\r\n"
 * Ký tự điền khác nhau theo writer ('a' cho W0, 'b' cho W1, 'c' cho W2) để khi
 * bản tin bị lẫn, nhìn log là biết chính xác byte nào của writer nào. */
void writer_task(void *argument)
{
    uint32_t id = (uint32_t)(uintptr_t)argument;//ép kiểu qua uintptr_t để không phải cấp phát bộ nhớ
    uint32_t seq = 0;
    char msg[MSG_LEN];
    const char fill = (char)('a' + (id % 26U)); //ràng trong a->z

    for (;;) {
        /* Header; snprintf luôn kết thúc bằng '\0' nên cắt phần dư rồi điền đè. */
        int n = snprintf(msg, sizeof msg, "W%lu #%05lu ", (unsigned long)id, (unsigned long)seq); //đúng 10 ký tự + \0

        //rào hờ
        if (n < 0)
            n = 0;
        //rào hờ lố khói giá trị max
        if (n > (int)(MSG_LEN - 2))
            n = (int)(MSG_LEN - 2);

        memset(&msg[n], fill, (size_t)(MSG_LEN - 2 - (size_t)n));//ghi đè ký tự 'fill' từ vị trí \0 cho đủ số lượng
        msg[MSG_LEN - 2] = '\r'; //them \r\n đúng format 
        msg[MSG_LEN - 1] = '\n';

        uart_guard_send(msg, MSG_LEN);
        seq++;

        vTaskDelay(pdMS_TO_TICKS(WRITER_PERIOD_MS));
    }
}