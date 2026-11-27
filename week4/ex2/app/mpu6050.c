/* Week 4 / Ex2 -- MPU6050 over I2C1.
 * Native FreeRTOS API only.
 */
#include "main.h"
#include "mpu6050.h"

#define MPU6050_ADDR        (0x68 << 1) //địa chỉ 7 bit, bit thấp nhất dành cho đọc/ghi
#define REG_PWR_MGMT_1       0x6B       //địa chỉ thanh ghi cần ghi để khởi động cảm biến
#define REG_ACCEL_XOUT_H     0x3B       //địa chỉ thanh ghi cần đọc để lấy dữ liệu

extern I2C_HandleTypeDef hi2c1;

void mpu6050_init(void)
{
    uint8_t zero = 0;
    //ghi 0 vào thanh ghi 0x6B quản lý nguồn của MPU6050 để khởi động, thoát chế độ ngủ
    HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR, REG_PWR_MGMT_1, 1, &zero, 1, 100); 
}

void mpu6050_read_sample(sensor_sample_t *out)
{
    uint8_t buf[14]; //chỗ chứa 14 byte đọc về

    HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR, REG_ACCEL_XOUT_H, 1, buf, 14, 100); //đọc 14 byte bắt đầu từ thanh ghi 0x3B

    out->accel_x = (int16_t)((buf[0] << 8) | buf[1]);
    out->accel_y = (int16_t)((buf[2] << 8) | buf[3]);
    out->accel_z = (int16_t)((buf[4] << 8) | buf[5]);
    out->gyro_x  = (int16_t)((buf[8] << 8) | buf[9]);
    out->gyro_y  = (int16_t)((buf[10] << 8) | buf[11]);
    out->gyro_z  = (int16_t)((buf[12] << 8) | buf[13]);
}

