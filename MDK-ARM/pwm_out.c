#include "pwm_out.h"
#include "motor_ctrl.h"

static TIM_HandleTypeDef s_pwm_tim;
static uint8_t s_pwm_initialized = 0U;

void pwm_out_init(void)//PWM 输出初始化
{
    GPIO_InitTypeDef gpio = {0};
    TIM_OC_InitTypeDef config = {0};

    if (s_pwm_initialized != 0U) {
        return;
    }

    __HAL_RCC_GPIOA_CLK_ENABLE();
    TIM1_PWM_TIMER_CLK_ENABLE();

    gpio.Pin = DRV_IN1_PIN | DRV_IN2_PIN | DRV_IN3_PIN;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = DRV_IN1_GPIO_AF;
    HAL_GPIO_Init(GPIOA, &gpio);

    s_pwm_tim.Instance = TIM1_PWM_TIMER_INSTANCE;
    s_pwm_tim.Init.Prescaler = TIM1_PWM_TIMER_PRESCALER;
    s_pwm_tim.Init.CounterMode = TIM_COUNTERMODE_UP;
    s_pwm_tim.Init.Period = TIM1_PWM_TIMER_PERIOD;
    s_pwm_tim.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    s_pwm_tim.Init.RepetitionCounter = 0U;
    s_pwm_tim.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    if (HAL_TIM_PWM_Init(&s_pwm_tim) != HAL_OK) {
        while (1) {
        }
    }

    config.OCMode = TIM_OCMODE_PWM1;
    config.Pulse = PWM_DUTY_MIN;
    config.OCPolarity = TIM_OCPOLARITY_HIGH;
    config.OCFastMode = TIM_OCFAST_DISABLE;

    if (HAL_TIM_PWM_ConfigChannel(&s_pwm_tim, &config, DRV_IN1_PWM_CHANNEL) != HAL_OK) {
        while (1) {
        }
    }

    if (HAL_TIM_PWM_ConfigChannel(&s_pwm_tim, &config, DRV_IN2_PWM_CHANNEL) != HAL_OK) {
        while (1) {
        }
    }

    if (HAL_TIM_PWM_ConfigChannel(&s_pwm_tim, &config, DRV_IN3_PWM_CHANNEL) != HAL_OK) {
        while (1) {
        }
    }

    if (HAL_TIM_PWM_Start(&s_pwm_tim, DRV_IN1_PWM_CHANNEL) != HAL_OK) {
        while (1) {
        }
    }

    if (HAL_TIM_PWM_Start(&s_pwm_tim, DRV_IN2_PWM_CHANNEL) != HAL_OK) {
        while (1) {
        }
    }

    if (HAL_TIM_PWM_Start(&s_pwm_tim, DRV_IN3_PWM_CHANNEL) != HAL_OK) {
        while (1) {
        }
    }

    HAL_NVIC_SetPriority(TIM1_PWM_TIMER_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(TIM1_PWM_TIMER_IRQn);
    pwm_out_enable_update_irq(0U);

    s_pwm_initialized = 1U;
}

void pwm_out_enable_update_irq(uint8_t enable)
{
    if (enable != 0U) {
        __HAL_TIM_SET_COUNTER(&s_pwm_tim, 0U);
        __HAL_TIM_CLEAR_FLAG(&s_pwm_tim, TIM_FLAG_UPDATE);
        __HAL_TIM_ENABLE_IT(&s_pwm_tim, TIM_IT_UPDATE);
        return;
    }

    __HAL_TIM_DISABLE_IT(&s_pwm_tim, TIM_IT_UPDATE);
    __HAL_TIM_CLEAR_FLAG(&s_pwm_tim, TIM_FLAG_UPDATE);
}


void pwm_duty_set(pwm_io_t io, uint16_t duty)//pwm设置
{
    uint32_t channel;

    if (duty > PWM_DUTY_MAX) {
        duty = PWM_DUTY_MAX;
    }

    switch (io) {
    case PWM_IO_IN1:
        channel = DRV_IN1_PWM_CHANNEL;
        break;

    case PWM_IO_IN2:
        channel = DRV_IN2_PWM_CHANNEL;
        break;

    case PWM_IO_IN3:
        channel = DRV_IN3_PWM_CHANNEL;
        break;

    default:
        return;
    }

    __HAL_TIM_SET_COMPARE(&s_pwm_tim, channel, duty);
}

void pwm_out_tim_irqhandler(void)//TIM1中断更新
{
    if ((__HAL_TIM_GET_FLAG(&s_pwm_tim, TIM_FLAG_UPDATE) != RESET) &&
        (__HAL_TIM_GET_IT_SOURCE(&s_pwm_tim, TIM_IT_UPDATE) != RESET)) {
        __HAL_TIM_CLEAR_IT(&s_pwm_tim, TIM_IT_UPDATE);
        MotorCtrl_PwmUpdateHandler();
    }
}
