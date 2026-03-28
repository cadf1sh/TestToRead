#ifndef MOTOR_SINE_OPENLOOP_H
#define MOTOR_SINE_OPENLOOP_H

#include "app_config.h"

/*
 * 正弦开环控制模块。
 *
 * 职责拆成两半：
 * - 10ms 低频任务里缓慢调整目标频率、目标幅值；
 * - 20kHz PWM 更新中断里按照 phase_acc_q16 / phase_step_q16 推进相位，
 *   然后把三相正弦值换算成 U/V/W 三路 PWM 占空比。
 */

void MotorSineOpenLoop_Init(void);
HAL_StatusTypeDef MotorSineOpenLoop_Start(void);
void MotorSineOpenLoop_Stop(void);
void MotorSineOpenLoop_Task10ms(void);

void MotorSineOpenLoop_SetReverse(uint8_t reverse);
void MotorSineOpenLoop_SetTargetFreqMilliHz(uint32_t freq_mhz);
void MotorSineOpenLoop_SetFreqRampMilliHzPer10ms(uint32_t ramp_mhz);
void MotorSineOpenLoop_SetTargetAmplitudePermille(uint16_t amp_permille);
void MotorSineOpenLoop_SetAmpRampPermillePer10ms(uint16_t ramp_permille);

uint8_t MotorSineOpenLoop_IsRunning(void);
uint8_t MotorSineOpenLoop_IsReverse(void);
uint32_t MotorSineOpenLoop_GetPhaseAccQ16(void);
uint32_t MotorSineOpenLoop_GetPhaseStepQ16(void);
uint32_t MotorSineOpenLoop_GetCurrentFreqMilliHz(void);
uint32_t MotorSineOpenLoop_GetTargetFreqMilliHz(void);
uint16_t MotorSineOpenLoop_GetCurrentAmplitudePermille(void);
uint16_t MotorSineOpenLoop_GetTargetAmplitudePermille(void);

#endif
