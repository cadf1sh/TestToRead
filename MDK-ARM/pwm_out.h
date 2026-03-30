#ifndef PWM_OUT_H
#define PWM_OUT_H

#include "app_config.h"

typedef enum
{
    PWM_IO_IN1 = 0,
    PWM_IO_IN2,
    PWM_IO_IN3
} pwm_io_t;

void pwm_out_init(void);
void pwm_out_enable_update_irq(uint8_t enable);
void pwm_duty_set(pwm_io_t io, uint16_t duty_0_to_1000);
void pwm_out_tim_irqhandler(void);

#endif
