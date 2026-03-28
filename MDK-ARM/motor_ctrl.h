#ifndef MOTOR_CTRL_H
#define MOTOR_CTRL_H

#include "app_config.h"

typedef enum
{
    MOTOR_DIR_FORWARD = 0, /* 按当前换相表/相位累加器的正方向推进。 */
    MOTOR_DIR_REVERSE = 1  /* 按相反方向推进。 */
} motor_direction_t;

typedef enum
{
    MOTOR_MODE_SIXSTEP = 0,    /* 10ms 任务里按步进节拍切换六步换相表。 */
    MOTOR_MODE_SINE_OPENLOOP   /* 10ms 任务调频调幅，20kHz 中断里更新正弦 PWM。 */
} motor_mode_t;

/*
 * 电机控制调度层接口。
 *
 * 这一层不直接碰 GPIO/TIM 寄存器，而是负责：
 * 1. 统一管理当前模式、方向、运行状态；
 * 2. 决定启动时走 six-step 还是正弦开环；
 * 3. 把 10ms 低频任务分发给对应模式；
 * 4. 把上层需要观察的状态整理成查询接口。
 */

void MotorCtrl_Init(void);
HAL_StatusTypeDef MotorCtrl_Start(void);
void MotorCtrl_Stop(void);
void MotorCtrl_Task10ms(void);

void MotorCtrl_SetMode(motor_mode_t mode);
motor_mode_t MotorCtrl_GetMode(void);
void MotorCtrl_SetDirection(motor_direction_t dir);
motor_direction_t MotorCtrl_GetDirection(void);

void MotorCtrl_SetSixStepStepTicks(uint16_t ticks);
uint16_t MotorCtrl_GetSixStepStepTicks(void);
void MotorCtrl_SetStartStep(uint8_t step_0_to_5);
uint8_t MotorCtrl_GetStartStep(void);
uint8_t MotorCtrl_GetCurrentStep(void);
uint16_t MotorCtrl_GetTickCounter(void);
uint8_t MotorCtrl_IsRunning(void);

void MotorCtrl_SineSetTargetFreqMilliHz(uint32_t freq_mhz);
void MotorCtrl_SineSetFreqRampMilliHzPer10ms(uint32_t ramp_mhz);
void MotorCtrl_SineSetAmplitudePermille(uint16_t amp_permille);
void MotorCtrl_SineSetAmpRampPermillePer10ms(uint16_t ramp_permille);
uint32_t MotorCtrl_SineGetCurrentFreqMilliHz(void);
uint32_t MotorCtrl_SineGetPhaseAccQ16(void);
uint32_t MotorCtrl_SineGetPhaseStepQ16(void);
uint16_t MotorCtrl_SineGetCurrentAmplitudePermille(void);

#endif
