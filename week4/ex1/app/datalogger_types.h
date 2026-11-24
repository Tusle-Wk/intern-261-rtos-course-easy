#ifndef DATALOGGER_TYPES_H
#define DATALOGGER_TYPES_H

#include <stdint.h>

/* Shared sample type for the whole mini project (ex1 design -> ex2
 * implementation -> ex3 measurement). Do not redefine this struct in a
 * later exercise -- it is the contract between ISR, queue and UART stages.
 *
 * TODO 1: add the fields your design needs (raw accel/gyro from MPU6050,
 *         a timestamp from the DWT cycle counter, anything else you listed
 *         in REPORT.md section 1). Keep it small -- it is copied by value
 *         through the queue on every sample. */
typedef struct {
    /* TODO: int16_t accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z; */
    uint32_t timestamp;   // DWT->CYCCNT lấy trong ISR (72 MHz)
    int16_t accel_x;      // raw MPU6050 accelerometer
    int16_t accel_y;
    int16_t accel_z;
    int16_t gyro_x;       // raw MPU6050 gyroscope
    int16_t gyro_y;
    int16_t gyro_z;
} sensor_sample_t;

#endif /* DATALOGGER_TYPES_H */
