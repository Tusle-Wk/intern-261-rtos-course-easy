#ifndef LOGGER_TASK_H
#define LOGGER_TASK_H

//tạo stream buffer, gọi trước khi tạo tasks
void logger_init(void);

//được đánh thức bởi ISR, đọc mẫu MPU6050 và đưa vào queue của mẫu
void sampler_task(void *argument);

 //lấy mẫu từ queue ra, lọc rồi ghi vào stream buffer, phải hoàn thành trước hạn ISR_TO_PROC_DEADLINES_US trung bình
void logger_task(void *argument);

//truyền ra uart khi đủ 64 mẫu trong stream buffer
void uart_task(void *argument);

#endif /* LOGGER_TASK_H */
