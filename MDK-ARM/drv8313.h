#ifndef DRV8313_H
#define DRV8313_H

#include "app_config.h"

typedef enum
{
    DRV_PHASE_U = 0,
    DRV_PHASE_V,
    DRV_PHASE_W
} drv_phase_t;

typedef enum
{
    DRV_PHASE_OFF = 0,
    DRV_PHASE_POSITIVE,
    DRV_PHASE_NEGATIVE
} drv_phase_state;

void DRV8313_Init(void);
void DRV8313_EnterSafeState(void);
HAL_StatusTypeDef DRV8313_Wakeup(void);
uint8_t DRV8313_IsAwake(void);
uint8_t DRV8313_IsFaultActive(void);
void DRV8313_EnableAllOutputs(void);
void DRV8313_SetPhaseState(drv_phase_t phase, drv_phase_state state);
void DRV8313_SetPhasePwmDuty(drv_phase_t phase, uint16_t duty_0_to_1000);
void DRV8313_AllPhaseOff(void);

#endif
