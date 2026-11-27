/* Week 4 / Ex2 -- batches samples from sample_queue and writes them to
 * UART in blocks of LOG_BLOCK_SAMPLES.
 * Native FreeRTOS API only.
 */
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "stream_buffer.h"

#include "main.h"
#include "params.h"
#include "datalogger_types.h"
#include "mpu6050.h"
#include "sampler_isr.h"
#include "logger_task.h"

#define BLOCK_BYTES  (LOG_BLOCK_SAMPLES * sizeof(sensor_sample_t)) //64x16=1024 byte

static StreamBufferHandle_t block_stream; //stream buffer
static uint8_t tx_block[BLOCK_BYTES]; //bộ đệm chép từ stream buffer sang

extern UART_HandleTypeDef huart1;

void logger_init(void)
{
    block_stream = xStreamBufferCreate(2 * BLOCK_BYTES, BLOCK_BYTES);
}

void sampler_task(void *argument)
{
    (void)argument;
    sensor_sample_t s;
    uint32_t timestamp;

    for (;;) {
        xTaskNotifyWait(0, 0, &timestamp, portMAX_DELAY); //ngủ đến khi ISR đánh thức, lấy timestamp ISR gửi.
        mpu6050_read_sample(&s); //đọc cảm biến (khoảng 390 us), điền 6 trục
        s.timestamp = timestamp; //gắn thời điểm lấy từ ISR
        xQueueSend(sample_queue, &s, 0); //đẩy vào queue, không chờ
    }
}

void logger_task(void *argument)
{
    (void)argument;
    sensor_sample_t s;
    static sensor_sample_t prev; //mẫu đã lọc lần trước

    for (;;) {
        xQueueReceive(sample_queue, &s, portMAX_DELAY);//ngủ đến khi có mẫu, lấy ra
        //lọc kết quả
        s.accel_x = prev.accel_x + ((s.accel_x - prev.accel_x) >> 2);
        s.accel_y = prev.accel_y + ((s.accel_y - prev.accel_y) >> 2);
        s.accel_z = prev.accel_z + ((s.accel_z - prev.accel_z) >> 2);
        s.gyro_x  = prev.gyro_x  + ((s.gyro_x  - prev.gyro_x)  >> 2);
        s.gyro_y  = prev.gyro_y  + ((s.gyro_y  - prev.gyro_y)  >> 2);
        s.gyro_z  = prev.gyro_z  + ((s.gyro_z  - prev.gyro_z)  >> 2);
        prev = s;//cập nhật prev cho lần sau

        if (xStreamBufferSpacesAvailable(block_stream) >= sizeof(s)) { //kiểm tra còn đủ 16 B trống -> để không ghi nửa mẫu
            xStreamBufferSend(block_stream, &s, sizeof(s), 0); //ghi 16 B, không chờ
        }
    }
}

void uart_task(void *argument)
{
    (void)argument;
    size_t got;

    for (;;) {
        got = 0; //đếm coi đã nhận được bao nhiêu byte cho khối này, do lúc uart gửi thì log đã thêm mẫu vào stream buffer rồi
        while (got < BLOCK_BYTES) {
            //lấy tối đa phần còn thiếu, ghi nối tiếp vào độ đệm tx_block đến khi đủ
            got += xStreamBufferReceive(block_stream, tx_block + got, BLOCK_BYTES - got, portMAX_DELAY);
        }
        HAL_UART_Transmit(&huart1, tx_block, BLOCK_BYTES, HAL_MAX_DELAY); //đủ 64 block mới truyền
    }
}