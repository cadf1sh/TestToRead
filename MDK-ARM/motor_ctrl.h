#ifndef MOTOR_CTRL_H
#define MOTOR_CTRL_H

#include "app_config.h"

typedef enum
{
    MOTOR_DIR_FORWARD = 0,
    MOTOR_DIR_REVERSE = 1
} motor_direction_t;

typedef enum
{
    MOTOR_MODE_SIXSTEP = 0,
    MOTOR_MODE_SINE_OPENLOOP
} motor_mode_t;

extern uint16_t Sine_Udate_hz;
void MotorCtrl_Init(void);
HAL_StatusTypeDef MotorCtrl_Start(void);
void MotorCtrl_Stop(void);
void MotorCtrl_Task(void);
void MotorCtrl_PwmUpdateHandler(void);

void MotorCtrl_SetMode(motor_mode_t mode);
motor_mode_t MotorCtrl_GetMode(void);
void MotorCtrl_SetDirection(motor_direction_t dir);
void MotorCtrl_SetSixStepStepTicks(uint16_t ticks);
uint8_t MotorCtrl_GetCurrentStep(void);
uint16_t MotorCtrl_GetTickCounter(void);
uint8_t MotorCtrl_IsRunning(void);

void MotorCtrl_SineSetTargetFreqMilliHz(uint32_t freq_mhz);
void MotorCtrl_SineSetFreqRampMilliHzPerTick(uint32_t ramp_mhz);
void MotorCtrl_SineSetAmplitudePermille(uint16_t amp_permille);
void MotorCtrl_SineSetAmpRampPermillePerTick(uint16_t ramp_permille);
uint32_t MotorCtrl_SineGetCurrentFreqMilliHz(void);
uint32_t MotorCtrl_SineGetPhaseAccQ16(void);
uint32_t MotorCtrl_SineGetPhaseStepQ16(void);
uint16_t MotorCtrl_SineGetCurrentAmplitudePermille(void);

#endif
