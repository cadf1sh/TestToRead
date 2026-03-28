#include "drv8313.h"
#include "drv8313_io.h"

typedef struct
{
    uint8_t awake;
    drv_phase_state_t phase_state[3];
    uint16_t phase_duty[3];
} drv8313_state_t;

static drv8313_state_t s_drv;

static drv8313_output_t DRV8313_MapPhaseToOutput(drv_phase_t phase)
{
    /*
     * Fixed wiring map:
     * U(RED)    -> OUT3 -> IN3/EN3
     * V(YELLOW) -> OUT2 -> IN2/EN2
     * W(BLUE)   -> OUT1 -> IN1/EN1
     */
    switch (phase) {
    case DRV_PHASE_U:
        return DRV8313_OUT3;

    case DRV_PHASE_V:
        return DRV8313_OUT2;

    case DRV_PHASE_W:
    default:
        return DRV8313_OUT1;
    }
}

static void DRV8313_ApplyOutputState(drv8313_output_t output, drv_phase_state_t state)
{
    if (state == DRV_PHASE_OFF) {
        DRV8313_IO_SetEnable(output, GPIO_PIN_RESET);
        DRV8313_IO_SetInput(output, GPIO_PIN_RESET);
        return;
    }

    DRV8313_IO_SetInput(output, (state == DRV_PHASE_POSITIVE) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    DRV8313_IO_SetEnable(output, GPIO_PIN_SET);
}

static void DRV8313_ApplyAllCachedPhaseStates(void)
{
    DRV8313_ApplyOutputState(DRV8313_MapPhaseToOutput(DRV_PHASE_U), s_drv.phase_state[DRV_PHASE_U]);
    DRV8313_ApplyOutputState(DRV8313_MapPhaseToOutput(DRV_PHASE_V), s_drv.phase_state[DRV_PHASE_V]);
    DRV8313_ApplyOutputState(DRV8313_MapPhaseToOutput(DRV_PHASE_W), s_drv.phase_state[DRV_PHASE_W]);
}

void DRV8313_Init(void)
{
    s_drv.awake = 0U;
    s_drv.phase_state[DRV_PHASE_U] = DRV_PHASE_OFF;
    s_drv.phase_state[DRV_PHASE_V] = DRV_PHASE_OFF;
    s_drv.phase_state[DRV_PHASE_W] = DRV_PHASE_OFF;
    s_drv.phase_duty[DRV_PHASE_U] = PWM_DUTY_MIN;
    s_drv.phase_duty[DRV_PHASE_V] = PWM_DUTY_MIN;
    s_drv.phase_duty[DRV_PHASE_W] = PWM_DUTY_MIN;

    DRV8313_IO_Init();
    DRV8313_EnterSafeState();
}

void DRV8313_EnterSafeState(void)
{
    DRV8313_AllPhaseOff();
    DRV8313_IO_SetNReset(GPIO_PIN_RESET);
    DRV8313_IO_SetNSleep(GPIO_PIN_RESET);
    s_drv.awake = 0U;
}

void DRV8313_Sleep(void)
{
    DRV8313_EnterSafeState();
}

HAL_StatusTypeDef DRV8313_Wakeup(void)
{
    DRV8313_AllPhaseOff();
    DRV8313_IO_SetNReset(GPIO_PIN_RESET);
    DRV8313_IO_SetNSleep(GPIO_PIN_RESET);
    HAL_Delay(DRV_WAKE_DELAY_MS);

    DRV8313_IO_SetNSleep(GPIO_PIN_SET);
    HAL_Delay(DRV_WAKE_DELAY_MS);

    DRV8313_IO_SetNReset(GPIO_PIN_SET);
    HAL_Delay(DRV_POST_WAKE_DELAY_MS);

    s_drv.awake = 1U;
    DRV8313_ApplyAllCachedPhaseStates();

    return DRV8313_IsFaultActive() ? HAL_ERROR : HAL_OK;
}

HAL_StatusTypeDef DRV8313_Reset(void)
{
    if (s_drv.awake == 0U) {
        return DRV8313_Wakeup();
    }

    DRV8313_IO_AllOutputsOff();
    DRV8313_IO_SetNReset(GPIO_PIN_RESET);
    HAL_Delay(DRV_RESET_PULSE_MS);
    DRV8313_IO_SetNReset(GPIO_PIN_SET);
    HAL_Delay(DRV_POST_WAKE_DELAY_MS);

    DRV8313_ApplyAllCachedPhaseStates();

    return DRV8313_IsFaultActive() ? HAL_ERROR : HAL_OK;
}

HAL_StatusTypeDef DRV8313_ClearFault(void)
{
    return DRV8313_Reset();
}

uint8_t DRV8313_IsAwake(void)
{
    return s_drv.awake;
}

uint8_t DRV8313_IsFaultActive(void)
{
    return (DRV8313_IO_ReadNFault() == DRV_NFAULT_ACTIVE_LEVEL) ? 1U : 0U;
}

void DRV8313_EnableAllOutputs(void)
{
    if (s_drv.awake == 0U) {
        return;
    }

    DRV8313_IO_SetEnable(DRV8313_OUT1, GPIO_PIN_SET);
    DRV8313_IO_SetEnable(DRV8313_OUT2, GPIO_PIN_SET);
    DRV8313_IO_SetEnable(DRV8313_OUT3, GPIO_PIN_SET);
}

void DRV8313_SetPhaseState(drv_phase_t phase, drv_phase_state_t state)
{
    if (phase > DRV_PHASE_W) {
        return;
    }

    s_drv.phase_state[phase] = state;
    s_drv.phase_duty[phase] = (state == DRV_PHASE_POSITIVE) ? PWM_DUTY_MAX : PWM_DUTY_MIN;

    if (s_drv.awake != 0U) {
        DRV8313_ApplyOutputState(DRV8313_MapPhaseToOutput(phase), state);
    }
}

drv_phase_state_t DRV8313_GetPhaseState(drv_phase_t phase)
{
    if (phase > DRV_PHASE_W) {
        return DRV_PHASE_OFF;
    }

    return s_drv.phase_state[phase];
}

void DRV8313_SetPhasePwmDuty(drv_phase_t phase, uint16_t duty_0_to_1000)
{
    if (phase > DRV_PHASE_W) {
        return;
    }

    s_drv.phase_duty[phase] = duty_0_to_1000;

    if (s_drv.awake != 0U) {
        DRV8313_IO_SetInputDuty(DRV8313_MapPhaseToOutput(phase), duty_0_to_1000);
    }
}

uint16_t DRV8313_GetPhasePwmDuty(drv_phase_t phase)
{
    if (phase > DRV_PHASE_W) {
        return PWM_DUTY_MIN;
    }

    return s_drv.phase_duty[phase];
}

void DRV8313_AllPhaseOff(void)
{
    s_drv.phase_state[DRV_PHASE_U] = DRV_PHASE_OFF;
    s_drv.phase_state[DRV_PHASE_V] = DRV_PHASE_OFF;
    s_drv.phase_state[DRV_PHASE_W] = DRV_PHASE_OFF;
    s_drv.phase_duty[DRV_PHASE_U] = PWM_DUTY_MIN;
    s_drv.phase_duty[DRV_PHASE_V] = PWM_DUTY_MIN;
    s_drv.phase_duty[DRV_PHASE_W] = PWM_DUTY_MIN;

    DRV8313_IO_AllOutputsOff();
}
