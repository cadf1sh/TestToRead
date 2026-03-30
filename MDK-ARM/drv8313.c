#include "drv8313.h"

static uint8_t drv_is_awake = 0U;//drv sleep 和reset 是否正常

static void DRV8313_WriteEnable(drv_phase_t phase, GPIO_PinState state)
{
    switch (phase) {
    case DRV_PHASE_U:
        HAL_GPIO_WritePin(DRV_EN3_GPIO_PORT, DRV_EN3_PIN, state);
        break;

    case DRV_PHASE_V:
        HAL_GPIO_WritePin(DRV_EN2_GPIO_PORT, DRV_EN2_PIN, state);
        break;

    case DRV_PHASE_W:
        HAL_GPIO_WritePin(DRV_EN1_GPIO_PORT, DRV_EN1_PIN, state);
        break;

    default:
        break;
    }
}

static void DRV8313_WriteInputDuty(drv_phase_t phase, uint16_t duty)
{
    switch (phase) {
    case DRV_PHASE_U:
        pwm_duty_set(PWM_IO_IN3, duty);
        break;

    case DRV_PHASE_V:
        pwm_duty_set(PWM_IO_IN2, duty);
        break;

    case DRV_PHASE_W:
        pwm_duty_set(PWM_IO_IN1, duty);
        break;

    default:
        break;
    }
}

void DRV8313_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    pwm_out_init();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    gpio.Pin = DRV_EN1_PIN | DRV_EN3_PIN | DRV_NSLEEP_PIN | DRV_NRESET_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);

    gpio.Pin = DRV_EN2_PIN;
    HAL_GPIO_Init(GPIOC, &gpio);

    gpio.Pin = DRV_NFAULT_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &gpio);

    drv_is_awake = 0U;
    DRV8313_EnterSafeState();
}

void DRV8313_EnterSafeState(void)
{
    DRV8313_AllPhaseOff();
    HAL_GPIO_WritePin(DRV_NRESET_GPIO_PORT, DRV_NRESET_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(DRV_NSLEEP_GPIO_PORT, DRV_NSLEEP_PIN, GPIO_PIN_RESET);
    drv_is_awake = 0U;
}

HAL_StatusTypeDef DRV8313_Wakeup(void)//状态管理，防止睡眠和复位
{
    DRV8313_AllPhaseOff();

    HAL_GPIO_WritePin(DRV_NRESET_GPIO_PORT, DRV_NRESET_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(DRV_NSLEEP_GPIO_PORT, DRV_NSLEEP_PIN, GPIO_PIN_RESET);
    HAL_Delay(DRV_WAKE_DELAY_MS);

    HAL_GPIO_WritePin(DRV_NSLEEP_GPIO_PORT, DRV_NSLEEP_PIN, GPIO_PIN_SET);
    HAL_Delay(DRV_WAKE_DELAY_MS);

    HAL_GPIO_WritePin(DRV_NRESET_GPIO_PORT, DRV_NRESET_PIN, GPIO_PIN_SET);
    HAL_Delay(DRV_POST_WAKE_DELAY_MS);

    drv_is_awake = 1U;
    return DRV8313_IsFaultActive() ? HAL_ERROR : HAL_OK;
}

uint8_t DRV8313_IsAwake(void)
{
    return drv_is_awake;
}

uint8_t DRV8313_IsFaultActive(void)
{
    return (HAL_GPIO_ReadPin(DRV_NFAULT_GPIO_PORT, DRV_NFAULT_PIN) == DRV_NFAULT_ACTIVE_LEVEL) ? 1U : 0U;
}

void DRV8313_EnableAllOutputs(void)//微步细分EN开关
{
    if (drv_is_awake == 0U) {
        return;
    }

    DRV8313_WriteEnable(DRV_PHASE_U, GPIO_PIN_SET);
    DRV8313_WriteEnable(DRV_PHASE_V, GPIO_PIN_SET);
    DRV8313_WriteEnable(DRV_PHASE_W, GPIO_PIN_SET);
}

void DRV8313_SetPhaseState(drv_phase_t phase, drv_phase_state state)//六步换向单步设置
{
    if ((phase > DRV_PHASE_W) || (drv_is_awake == 0U)) {
        return;
    }

    if (state == DRV_PHASE_OFF) {
        DRV8313_WriteEnable(phase, GPIO_PIN_RESET);
        DRV8313_WriteInputDuty(phase, PWM_DUTY_MIN);
        return;
    }

    DRV8313_WriteInputDuty(phase, (state == DRV_PHASE_POSITIVE) ? PWM_DUTY_MAX : PWM_DUTY_MIN);
    DRV8313_WriteEnable(phase, GPIO_PIN_SET);
}

void DRV8313_SetPhasePwmDuty(drv_phase_t phase, uint16_t duty)//微步细分PWM设置
{
    if ((phase > DRV_PHASE_W) || (drv_is_awake == 0U)) {
        return;
    }

    DRV8313_WriteInputDuty(phase, duty);
}

void DRV8313_AllPhaseOff(void)
{
    DRV8313_WriteEnable(DRV_PHASE_U, GPIO_PIN_RESET);
    DRV8313_WriteEnable(DRV_PHASE_V, GPIO_PIN_RESET);
    DRV8313_WriteEnable(DRV_PHASE_W, GPIO_PIN_RESET);

    DRV8313_WriteInputDuty(DRV_PHASE_U, PWM_DUTY_MIN);
    DRV8313_WriteInputDuty(DRV_PHASE_V, PWM_DUTY_MIN);
    DRV8313_WriteInputDuty(DRV_PHASE_W, PWM_DUTY_MIN);
}
