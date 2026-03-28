#include "drv8313_io.h"
#include "pwm_out.h"

static void DRV8313_IO_InitOutput(GPIO_TypeDef *port, uint16_t pin)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = pin;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(port, &gpio);
}

static void DRV8313_IO_InitInputPullup(GPIO_TypeDef *port, uint16_t pin)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = pin;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(port, &gpio);
}

static pwm_io_t DRV8313_IO_MapOutputToPwmIo(drv8313_output_t output)
{
    switch (output) {
    case DRV8313_OUT1:
        return PWM_IO_IN1;

    case DRV8313_OUT2:
        return PWM_IO_IN2;

    case DRV8313_OUT3:
    default:
        return PWM_IO_IN3;
    }
}

void DRV8313_IO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    pwm_out_init();
    DRV8313_IO_InitOutput(DRV_EN1_GPIO_PORT, DRV_EN1_PIN);
    DRV8313_IO_InitOutput(DRV_EN2_GPIO_PORT, DRV_EN2_PIN);
    DRV8313_IO_InitOutput(DRV_EN3_GPIO_PORT, DRV_EN3_PIN);
    DRV8313_IO_InitOutput(DRV_NSLEEP_GPIO_PORT, DRV_NSLEEP_PIN);
    DRV8313_IO_InitOutput(DRV_NRESET_GPIO_PORT, DRV_NRESET_PIN);
    DRV8313_IO_InitInputPullup(DRV_NFAULT_GPIO_PORT, DRV_NFAULT_PIN);

    DRV8313_IO_AllOutputsOff();
    DRV8313_IO_SetNSleep(GPIO_PIN_RESET);
    DRV8313_IO_SetNReset(GPIO_PIN_RESET);
}

void DRV8313_IO_SetEnable(drv8313_output_t output, GPIO_PinState state)
{
    switch (output) {
    case DRV8313_OUT1:
        HAL_GPIO_WritePin(DRV_EN1_GPIO_PORT, DRV_EN1_PIN, state);
        break;

    case DRV8313_OUT2:
        HAL_GPIO_WritePin(DRV_EN2_GPIO_PORT, DRV_EN2_PIN, state);
        break;

    case DRV8313_OUT3:
        HAL_GPIO_WritePin(DRV_EN3_GPIO_PORT, DRV_EN3_PIN, state);
        break;

    default:
        break;
    }
}

void DRV8313_IO_SetInput(drv8313_output_t output, GPIO_PinState state)
{
    DRV8313_IO_SetInputDuty(output, (state == GPIO_PIN_SET) ? PWM_DUTY_MAX : PWM_DUTY_MIN);
}

void DRV8313_IO_SetInputDuty(drv8313_output_t output, uint16_t duty_0_to_1000)
{
    switch (output) {
    case DRV8313_OUT1:
        pwm_duty_set(DRV8313_IO_MapOutputToPwmIo(DRV8313_OUT1), duty_0_to_1000);
        break;

    case DRV8313_OUT2:
        pwm_duty_set(DRV8313_IO_MapOutputToPwmIo(DRV8313_OUT2), duty_0_to_1000);
        break;

    case DRV8313_OUT3:
        pwm_duty_set(DRV8313_IO_MapOutputToPwmIo(DRV8313_OUT3), duty_0_to_1000);
        break;

    default:
        break;
    }
}

uint16_t DRV8313_IO_GetInputDuty(drv8313_output_t output)
{
    switch (output) {
    case DRV8313_OUT1:
        return pwm_duty_get(DRV8313_IO_MapOutputToPwmIo(DRV8313_OUT1));

    case DRV8313_OUT2:
        return pwm_duty_get(DRV8313_IO_MapOutputToPwmIo(DRV8313_OUT2));

    case DRV8313_OUT3:
        return pwm_duty_get(DRV8313_IO_MapOutputToPwmIo(DRV8313_OUT3));

    default:
        return PWM_DUTY_MIN;
    }
}

void DRV8313_IO_AllOutputsOff(void)
{
    DRV8313_IO_SetEnable(DRV8313_OUT1, GPIO_PIN_RESET);
    DRV8313_IO_SetEnable(DRV8313_OUT2, GPIO_PIN_RESET);
    DRV8313_IO_SetEnable(DRV8313_OUT3, GPIO_PIN_RESET);

    DRV8313_IO_SetInput(DRV8313_OUT1, GPIO_PIN_RESET);
    DRV8313_IO_SetInput(DRV8313_OUT2, GPIO_PIN_RESET);
    DRV8313_IO_SetInput(DRV8313_OUT3, GPIO_PIN_RESET);
}

void DRV8313_IO_SetNSleep(GPIO_PinState state)
{
    HAL_GPIO_WritePin(DRV_NSLEEP_GPIO_PORT, DRV_NSLEEP_PIN, state);
}

void DRV8313_IO_SetNReset(GPIO_PinState state)
{
    HAL_GPIO_WritePin(DRV_NRESET_GPIO_PORT, DRV_NRESET_PIN, state);
}

GPIO_PinState DRV8313_IO_ReadNFault(void)
{
    return HAL_GPIO_ReadPin(DRV_NFAULT_GPIO_PORT, DRV_NFAULT_PIN);
}
