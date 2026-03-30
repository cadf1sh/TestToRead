#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "stm32f4xx_hal.h"

/* TIM4task 100mHz/(9999+1)/(9+1)=1kHz*/
#define TIM4_TIMER_PRESCALER                  9999U
#define TIM4_TIMER_PERIOD                     9U
#define TIM4_TIMER_INSTANCE                   TIM4
#define TIM4_TIMER_CLK_ENABLE()               __HAL_RCC_TIM4_CLK_ENABLE()
#define TIM4_TIMER_IRQn                       TIM4_IRQn

/* TIM1PWM 100mhz/(4+1)/(999+1)=20khz*/
#define TIM1_PWM_TIMER_PRESCALER                   4U
#define TIM1_PWM_TIMER_PERIOD                      999U
#define TIM1_PWM_TIMER_INSTANCE                    TIM1
#define TIM1_PWM_TIMER_CLK_ENABLE()                __HAL_RCC_TIM1_CLK_ENABLE()
#define TIM1_PWM_TIMER_IRQn                        TIM1_UP_TIM10_IRQn

/* PWM限幅 */
#define PWM_DUTY_MIN                              0
#define PWM_DUTY_MAX                              1000
#define PWM_DUTY_MID                          500

/* LED2红灯 */
#define Alive_GPIO_PORT                       GPIOA
#define Alive_PIN                             GPIO_PIN_5

/* 启动模式选择 */
#define STARTUP_MODE_SIXSTEP                  0
#define STARTUP_MODE_SINE_OPENLOOP            1
#define STARTUP_MOTOR_MODE                    STARTUP_MODE_SINE_OPENLOOP

/* 六步换向 */
#define MOTOR_OPEN_LOOP_STEP_TICKS_DEFAULT        1

/* 微步细分 */
#define SINE_TABLE_SIZE                     96//表大小
#define SINE_PHASE_SHIFT_120_DEG            32
#define SINE_DEFAULT_target_freq          3000
#define SINE_DEFAULT_freq_ramp             200
#define SINE_DEFAULT_target_amp             1000
#define SINE_DEFAULT_AMP_RAMP_PER_TICK      50U

/* GPIO */
#define DRV_EN1_GPIO_PORT                         GPIOB
#define DRV_EN1_PIN                               GPIO_PIN_4
#define DRV_EN2_GPIO_PORT                         GPIOC
#define DRV_EN2_PIN                               GPIO_PIN_7
#define DRV_EN3_GPIO_PORT                         GPIOB
#define DRV_EN3_PIN                               GPIO_PIN_10

/* IN1/IN2/IN3  TIM1_CH1/CH2/CH3 */
#define DRV_IN1_GPIO_PORT                         GPIOA
#define DRV_IN1_PIN                               GPIO_PIN_8
#define DRV_IN1_GPIO_AF                           GPIO_AF1_TIM1
#define DRV_IN1_PWM_CHANNEL                       TIM_CHANNEL_1

#define DRV_IN2_GPIO_PORT                         GPIOA
#define DRV_IN2_PIN                               GPIO_PIN_9
#define DRV_IN2_GPIO_AF                           GPIO_AF1_TIM1
#define DRV_IN2_PWM_CHANNEL                       TIM_CHANNEL_2

#define DRV_IN3_GPIO_PORT                         GPIOA
#define DRV_IN3_PIN                               GPIO_PIN_10
#define DRV_IN3_GPIO_AF                           GPIO_AF1_TIM1
#define DRV_IN3_PWM_CHANNEL                       TIM_CHANNEL_3

/* Sleep / reset / fault */
#define DRV_NSLEEP_GPIO_PORT                      GPIOB
#define DRV_NSLEEP_PIN                            GPIO_PIN_9
#define DRV_NRESET_GPIO_PORT                      GPIOB
#define DRV_NRESET_PIN                            GPIO_PIN_8
#define DRV_NFAULT_GPIO_PORT                      GPIOB
#define DRV_NFAULT_PIN                            GPIO_PIN_6

#define DRV_NFAULT_ACTIVE_LEVEL                   GPIO_PIN_RESET

#define DRV_RESET_PULSE_MS                        1
#define DRV_WAKE_DELAY_MS                         1
#define DRV_POST_WAKE_DELAY_MS                    2


#include <stdio.h>
#include "stm32f4xx_hal.h"
#include "main.h"

#include "drv8313.h"
#include "motor_ctrl.h"
#include "my_uart.h"
#include "pwm_out.h"


#endif
