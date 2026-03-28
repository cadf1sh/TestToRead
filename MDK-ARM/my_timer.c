#include "my_timer.h"

static TIM_HandleTypeDef htim_task;
static my_timer_callback_t s_timer_callback = 0;

void my_Timer10ms_Init(void)
{
    APP_TASK_TIMER_CLK_ENABLE();

    htim_task.Instance = APP_TASK_TIMER_INSTANCE;
    htim_task.Init.Prescaler = APP_TASK_TIMER_PRESCALER;
    htim_task.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim_task.Init.Period = APP_TASK_TIMER_PERIOD;
    htim_task.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim_task.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_Base_Init(&htim_task) != HAL_OK) {
        while (1) {
        }
    }

    HAL_NVIC_SetPriority(APP_TASK_TIMER_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(APP_TASK_TIMER_IRQn);
}

void my_Timer10ms_Start(void)
{
    if (HAL_TIM_Base_Start_IT(&htim_task) != HAL_OK) {
        while (1) {
        }
    }
}

void my_Timer10ms_RegisterCallback(my_timer_callback_t callback)
{
    s_timer_callback = callback;
}

void APP_TASK_TIMER_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim_task);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if ((htim->Instance == APP_TASK_TIMER_INSTANCE) && (s_timer_callback != 0)) {
        s_timer_callback();
    }
}
