#include "pwm_out.h"

static TIM_HandleTypeDef s_htim_pwm;
static pwm_update_callback_t s_update_callback = 0;
static uint16_t s_pwm_duty[3];
static uint8_t s_pwm_initialized = 0U;

static uint16_t PWM_OUT_ClampDuty(uint16_t pwm)
{
    if (pwm > PWM_DUTY_MAX) {
        return PWM_DUTY_MAX;
    }

    return pwm;
}

static uint32_t PWM_OUT_MapChannel(pwm_io_t io)
{
    switch (io) {
    case PWM_IO_IN1:
        return DRV_IN1_PWM_CHANNEL;

    case PWM_IO_IN2:
        return DRV_IN2_PWM_CHANNEL;

    case PWM_IO_IN3:
    default:
        return DRV_IN3_PWM_CHANNEL;
    }
}

static void PWM_OUT_InitPins(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    gpio.Pin = DRV_IN1_PIN | DRV_IN2_PIN | DRV_IN3_PIN;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = DRV_IN1_GPIO_AF;
    HAL_GPIO_Init(GPIOA, &gpio);
}

static void PWM_OUT_ConfigChannel(uint32_t channel, uint32_t pulse)
{
    TIM_OC_InitTypeDef config = {0};

    config.OCMode = TIM_OCMODE_PWM1;
    config.Pulse = pulse;
    config.OCPolarity = TIM_OCPOLARITY_HIGH;
    config.OCFastMode = TIM_OCFAST_DISABLE;

    if (HAL_TIM_PWM_ConfigChannel(&s_htim_pwm, &config, channel) != HAL_OK) {
        while (1) {
        }
    }
}

void pwm_out_init(void)
{
    if (s_pwm_initialized != 0U) {
        return;
    }

    APP_PWM_TIMER_CLK_ENABLE();
    PWM_OUT_InitPins();

    s_htim_pwm.Instance = APP_PWM_TIMER_INSTANCE;
    s_htim_pwm.Init.Prescaler = APP_PWM_TIMER_PRESCALER;
    s_htim_pwm.Init.CounterMode = TIM_COUNTERMODE_UP;
    s_htim_pwm.Init.Period = APP_PWM_TIMER_PERIOD;
    s_htim_pwm.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    s_htim_pwm.Init.RepetitionCounter = 0U;
    s_htim_pwm.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    if (HAL_TIM_PWM_Init(&s_htim_pwm) != HAL_OK) {
        while (1) {
        }
    }

    PWM_OUT_ConfigChannel(DRV_IN1_PWM_CHANNEL, PWM_DUTY_MIN);
    PWM_OUT_ConfigChannel(DRV_IN2_PWM_CHANNEL, PWM_DUTY_MIN);
    PWM_OUT_ConfigChannel(DRV_IN3_PWM_CHANNEL, PWM_DUTY_MIN);

    if (HAL_TIM_PWM_Start(&s_htim_pwm, DRV_IN1_PWM_CHANNEL) != HAL_OK) {
        while (1) {
        }
    }

    if (HAL_TIM_PWM_Start(&s_htim_pwm, DRV_IN2_PWM_CHANNEL) != HAL_OK) {
        while (1) {
        }
    }

    if (HAL_TIM_PWM_Start(&s_htim_pwm, DRV_IN3_PWM_CHANNEL) != HAL_OK) {
        while (1) {
        }
    }

    s_pwm_duty[PWM_IO_IN1] = PWM_DUTY_MIN;
    s_pwm_duty[PWM_IO_IN2] = PWM_DUTY_MIN;
    s_pwm_duty[PWM_IO_IN3] = PWM_DUTY_MIN;

    HAL_NVIC_SetPriority(APP_PWM_TIMER_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(APP_PWM_TIMER_IRQn);
    pwm_out_enable_update_irq(0U);

    s_pwm_initialized = 1U;
}

void pwm_out_register_update_callback(pwm_update_callback_t callback)
{
    s_update_callback = callback;
}

void pwm_out_enable_update_irq(uint8_t enable)
{
    if (enable != 0U) {
        __HAL_TIM_SET_COUNTER(&s_htim_pwm, 0U);
        __HAL_TIM_CLEAR_FLAG(&s_htim_pwm, TIM_FLAG_UPDATE);
        __HAL_TIM_ENABLE_IT(&s_htim_pwm, TIM_IT_UPDATE);
        return;
    }

    __HAL_TIM_DISABLE_IT(&s_htim_pwm, TIM_IT_UPDATE);
    __HAL_TIM_CLEAR_FLAG(&s_htim_pwm, TIM_FLAG_UPDATE);
}

void pwm_duty_set(pwm_io_t io, uint16_t pwm)
{
    uint16_t clamped_duty;

    if (io > PWM_IO_IN3) {
        return;
    }

    clamped_duty = PWM_OUT_ClampDuty(pwm);
    s_pwm_duty[io] = clamped_duty;

    __HAL_TIM_SET_COMPARE(&s_htim_pwm, PWM_OUT_MapChannel(io), clamped_duty);
}

uint16_t pwm_duty_get(pwm_io_t io)
{
    if (io > PWM_IO_IN3) {
        return PWM_DUTY_MIN;
    }

    return s_pwm_duty[io];
}

uint32_t pwm_out_get_update_rate_hz(void)
{
    return APP_PWM_UPDATE_HZ;
}

void pwm_out_tim_irqhandler(void)
{
    if ((__HAL_TIM_GET_FLAG(&s_htim_pwm, TIM_FLAG_UPDATE) != RESET) &&
        (__HAL_TIM_GET_IT_SOURCE(&s_htim_pwm, TIM_IT_UPDATE) != RESET)) {
        __HAL_TIM_CLEAR_IT(&s_htim_pwm, TIM_IT_UPDATE);

        if (s_update_callback != 0) {
            s_update_callback();
        }
    }
}
