/* Week 2 / Ex1 — reads MPU6050 over I2C1 every SAMPLE_PERIOD_MS, pushes a
 * raw_sample_t into queue_raw.
 */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "main.h"
#include "params.h"
#include "pipeline.h"

extern I2C_HandleTypeDef hi2c1;

#define MPU6050_ADDR         (0x68 << 1)   //AD0 = GND; HAL dùng địa chỉ đã dịch trái
#define MPU_REG_WHO_AM_I     0x75
#define MPU_REG_PWR_MGMT_1   0x6B
#define MPU_REG_ACCEL_XOUT_H 0x3B

//Timeout của xQueueSend: đổi giữa 0 / pdMS_TO_TICKS(2) / portMAX_DELAY khi làm
//bước 7 (có thể truyền bằng -DSENSOR_SEND_TIMEOUT=...). */
#ifndef SENSOR_SEND_TIMEOUT
#define SENSOR_SEND_TIMEOUT  (0)
#endif

//debug
volatile uint32_t raw_peak = 0;
#define TICK_LOG_LEN 200
volatile uint32_t send_tick[TICK_LOG_LEN];
volatile uint32_t send_n = 0;

volatile uint32_t sensor_drops = 0;   //số lần xQueueSend thất bại (queue_raw đầy)
volatile uint32_t i2c_errors   = 0;

void sensor_task(void *argument)
{
    (void)argument;
    uint32_t seq = 0;

    //Khởi tạo một lần, ngoài vòng lặp
    uint8_t who = 0, wake = 0x00;
    if (HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR, MPU_REG_WHO_AM_I,
                         I2C_MEMADD_SIZE_8BIT, &who, 1, 100) != HAL_OK || who != 0x68) {
        Error_Handler();
    }
    //MPU6050 mặc định ở sleep -> ghi 0 vào PWR_MGMT_1 để đánh thức
    if (HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR, MPU_REG_PWR_MGMT_1,
                          I2C_MEMADD_SIZE_8BIT, &wake, 1, 100) != HAL_OK) {
        Error_Handler();
    }

    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        raw_sample_t s;
        s.seq = seq++;          //tăng kể cả khi mẫu bị rớt -> downstream thấy "lỗ" seq

        uint8_t b[6];
        if (HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR, MPU_REG_ACCEL_XOUT_H,
                             I2C_MEMADD_SIZE_8BIT, b, 6, 10) == HAL_OK) {
            s.ax = (int16_t)((b[0] << 8) | b[1]);   //MPU6050: big-endian
            s.ay = (int16_t)((b[2] << 8) | b[3]);
            s.az = (int16_t)((b[4] << 8) | b[5]);

             s.ax = ((s.seq / 100) % 2) ? 10000 : 0;  s.ay = 0;  s.az = 0;   // test cho câu 6
            if (xQueueSend(queue_raw, &s, SENSOR_SEND_TIMEOUT) != pdPASS) {
                sensor_drops++; //queue đầy + hết timeout: mẫu này mất
            }
            //debug
            UBaseType_t d = uxQueueMessagesWaiting(queue_raw);
            if (d > raw_peak) raw_peak = d;
            if (send_n < TICK_LOG_LEN) send_tick[send_n++] = xTaskGetTickCount();   

        } else {
            i2c_errors++;
            //I2C hay kẹt cờ BUSY sau lỗi -> reset ngoại vi để lần đọc sau chạy lại
            HAL_I2C_DeInit(&hi2c1);
            HAL_I2C_Init(&hi2c1);
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }
}