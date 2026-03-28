#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "stm32f4xx_hal.h"

/*
 * 工程级配置总表。
 *
 * 这里集中放了三类信息：
 * 1. 调度节拍：10ms 任务定时器、20kHz PWM 定时器；
 * 2. 控制约定：PWM 统一使用 0~1000 的占空比抽象、默认启动模式；
 * 3. 硬件映射：DRV8313 的 EN/IN/nSLEEP/nRESET/nFAULT 分别接到哪些脚。
 *
 * main.c、my_timer.c、pwm_out.c、drv8313_io.c、motor_sine_openloop.c
 * 都会直接依赖这里的宏，所以这个文件相当于“应用层和板级层共享的地基”。
 */

/* 10ms 低频任务定时器：100MHz / (9999 + 1) / (99 + 1) = 100Hz */
#define APP_TASK_TIMER_PRESCALER                     9999U
#define APP_TASK_TIMER_PERIOD                        99U
#define APP_TASK_TIMER_INSTANCE                      TIM4
#define APP_TASK_TIMER_CLK_ENABLE()                  __HAL_RCC_TIM4_CLK_ENABLE()
#define APP_TASK_TIMER_IRQn                          TIM4_IRQn
#define APP_TASK_TIMER_IRQHandler                    TIM4_IRQHandler

/* PWM 定时器：100MHz / (4 + 1) / (999 + 1) = 20kHz */
#define APP_PWM_TIMER_PRESCALER                      4U
#define APP_PWM_TIMER_PERIOD                         999U
#define APP_PWM_TIMER_INSTANCE                       TIM1
#define APP_PWM_TIMER_CLK_ENABLE()                   __HAL_RCC_TIM1_CLK_ENABLE()
#define APP_PWM_TIMER_IRQn                           TIM1_UP_TIM10_IRQn
#define APP_PWM_UPDATE_HZ                            20000U

/*
 * 当前工程统一把 PWM 抽象成 0~1000：
 * - 0    对应 0%
 * - 500  对应 50%，也就是正弦调制里的“中点”
 * - 1000 对应 100%
 *
 * 这样做的好处是，上层控制代码不用直接关心 TIM1 的 ARR=999，
 * 所有模块都按“千分比占空比”交流，最后由 pwm_out.c 映射到 CCR。
 */
#define PWM_DUTY_MIN                                 0U
#define PWM_DUTY_MAX                                 1000U
#define PWM_DUTY_NEUTRAL                             500U

/* Heartbeat LED on Nucleo D13 */
#define APP_HEARTBEAT_GPIO_PORT                      GPIOA
#define APP_HEARTBEAT_PIN                            GPIO_PIN_5

/* Startup mode selector for bring-up */
#define APP_STARTUP_MODE_SIXSTEP                     0U
#define APP_STARTUP_MODE_SINE_OPENLOOP               1U
#define APP_STARTUP_MOTOR_MODE                       APP_STARTUP_MODE_SINE_OPENLOOP

/* 六步换向默认步进节拍：10 x 10ms = 100ms 换一次相。 */
#define MOTOR_OPEN_LOOP_STEP_TICKS_DEFAULT           10U

/*
 * 正弦开环默认参数。
 * 96 点表意味着 360 度被离散成 96 份，因此 120 度相移对应 32 点。
 */
#define MOTOR_SINE_TABLE_SIZE                        96U
#define MOTOR_SINE_PHASE_SHIFT_120_DEG               32U
#define MOTOR_SINE_DEFAULT_TARGET_FREQ_MHZ           3000U
#define MOTOR_SINE_DEFAULT_FREQ_RAMP_MHZ_PER_10MS    200U
#define MOTOR_SINE_DEFAULT_TARGET_AMP_PERMILLE       700U
#define MOTOR_SINE_DEFAULT_AMP_RAMP_PER_10MS         50U

/* EN outputs remain GPIO */
/* EN1 -> D5  -> PB4  -> OUT1 (W/BLUE) */
/* EN2 -> D9  -> PC7  -> OUT2 (V/YELLOW) */
/* EN3 -> D6  -> PB10 -> OUT3 (U/RED) */
#define DRV_EN1_GPIO_PORT                            GPIOB
#define DRV_EN1_PIN                                  GPIO_PIN_4
#define DRV_EN2_GPIO_PORT                            GPIOC
#define DRV_EN2_PIN                                  GPIO_PIN_7
#define DRV_EN3_GPIO_PORT                            GPIOB
#define DRV_EN3_PIN                                  GPIO_PIN_10

/* IN inputs remapped to one 3-channel PWM timer: TIM1_CH1/CH2/CH3 */
/* IN1 -> D7  -> PA8  -> TIM1_CH1 -> OUT1 (W/BLUE) */
/* IN2 -> D8  -> PA9  -> TIM1_CH2 -> OUT2 (V/YELLOW) */
/* IN3 -> D2  -> PA10 -> TIM1_CH3 -> OUT3 (U/RED) */
#define DRV_IN1_GPIO_PORT                            GPIOA
#define DRV_IN1_PIN                                  GPIO_PIN_8
#define DRV_IN1_GPIO_AF                              GPIO_AF1_TIM1
#define DRV_IN1_PWM_CHANNEL                          TIM_CHANNEL_1

#define DRV_IN2_GPIO_PORT                            GPIOA
#define DRV_IN2_PIN                                  GPIO_PIN_9
#define DRV_IN2_GPIO_AF                              GPIO_AF1_TIM1
#define DRV_IN2_PWM_CHANNEL                          TIM_CHANNEL_2

#define DRV_IN3_GPIO_PORT                            GPIOA
#define DRV_IN3_PIN                                  GPIO_PIN_10
#define DRV_IN3_GPIO_AF                              GPIO_AF1_TIM1
#define DRV_IN3_PWM_CHANNEL                          TIM_CHANNEL_3

/* Sleep / reset moved away from TIM1 channels */
/* nSLEEP -> D14 -> PB9  (active low) */
/* nRESET -> D15 -> PB8  (active low) */
/* nFAULT -> D10 -> PB6  (active low input) */
#define DRV_NSLEEP_GPIO_PORT                         GPIOB
#define DRV_NSLEEP_PIN                               GPIO_PIN_9
#define DRV_NRESET_GPIO_PORT                         GPIOB
#define DRV_NRESET_PIN                               GPIO_PIN_8
#define DRV_NFAULT_GPIO_PORT                         GPIOB
#define DRV_NFAULT_PIN                               GPIO_PIN_6

#define DRV_NFAULT_ACTIVE_LEVEL                      GPIO_PIN_RESET

/* Conservative delays for bring-up */
#define DRV_RESET_PULSE_MS                           1U
#define DRV_WAKE_DELAY_MS                            1U
#define DRV_POST_WAKE_DELAY_MS                       2U

#endif
