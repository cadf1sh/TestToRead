#ifndef PWM_OUT_H
#define PWM_OUT_H

#include "app_config.h"

typedef enum
{
    PWM_IO_IN1 = 0, /* 对应 DRV8313 的 IN1，也就是 TIM1_CH1。 */
    PWM_IO_IN2,     /* 对应 DRV8313 的 IN2，也就是 TIM1_CH2。 */
    PWM_IO_IN3      /* 对应 DRV8313 的 IN3，也就是 TIM1_CH3。 */
} pwm_io_t;

typedef void (*pwm_update_callback_t)(void);

/*
 * PWM 输出抽象层。
 *
 * 上层模块统一用 0~1000 的 duty 来表达占空比，
 * 这一层负责：
 * 1. 初始化 TIM1 三路 PWM；
 * 2. 把 duty 映射到对应 CCR；
 * 3. 提供 20kHz 更新中断回调，让正弦模块挂接高频任务。
 */

void pwm_out_init(void);
void pwm_out_register_update_callback(pwm_update_callback_t callback);
void pwm_out_enable_update_irq(uint8_t enable);
void pwm_duty_set(pwm_io_t io, uint16_t duty_0_to_1000);
uint16_t pwm_duty_get(pwm_io_t io);
uint32_t pwm_out_get_update_rate_hz(void);
void pwm_out_tim_irqhandler(void);

#endif
