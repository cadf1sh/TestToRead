#include "app_config.h"


static void SystemClock_Config(void);
static void Error_Handler(void);
static void HeartbeatLed_Init(void);
static void TaskTimer_Init(void);
static void TaskTimer_Start(void);
static void ControlTask_Callback(void);

static TIM_HandleTypeDef s_task_tim;
static volatile uint8_t is_fault = 0U;//故障标志

int main(void)
{
    HAL_Init();
    HeartbeatLed_Init();
    SystemClock_Config();

    my_Uart_Init();
    printf("boot\r\n");

    DRV8313_Init();
    MotorCtrl_Init();

    if (STARTUP_MOTOR_MODE == STARTUP_MODE_SINE_OPENLOOP) {
        MotorCtrl_SetMode(MOTOR_MODE_SINE_OPENLOOP);
        MotorCtrl_SetDirection(MOTOR_DIR_FORWARD);
        MotorCtrl_SineSetTargetFreqMilliHz(SINE_DEFAULT_target_freq);
        MotorCtrl_SineSetFreqRampMilliHzPerTick(SINE_DEFAULT_freq_ramp);
        MotorCtrl_SineSetAmplitudePermille(SINE_DEFAULT_target_amp);
        MotorCtrl_SineSetAmpRampPermillePerTick(SINE_DEFAULT_AMP_RAMP_PER_TICK);
        printf("startup mode=sine_openloop\r\n");
    } else {
        MotorCtrl_SetMode(MOTOR_MODE_SIXSTEP);
        MotorCtrl_SetDirection(MOTOR_DIR_FORWARD);
        MotorCtrl_SetSixStepStepTicks(MOTOR_OPEN_LOOP_STEP_TICKS_DEFAULT);
        printf("startup mode=sixstep\r\n");
    }

    HAL_Delay(5);

    TaskTimer_Init();
    TaskTimer_Start();

    if (MotorCtrl_Start() != HAL_OK) {
        is_fault = 1U;
        printf("start failed fault=%u\r\n", DRV8313_IsFaultActive());
    }

    while (1) {
        static uint32_t last_log_ms = 0U;
        uint32_t now = HAL_GetTick();

        if ((now - last_log_ms) >= 200U) {
            last_log_ms = now;

            if (MotorCtrl_GetMode() == MOTOR_MODE_SINE_OPENLOOP) {
                printf("mode=sine run=%u freq=%lu amp=%u phase_step=%lu phase_acc=%lu awake=%u fault=%u\r\n",
                       MotorCtrl_IsRunning(),
                       (unsigned long)MotorCtrl_SineGetCurrentFreqMilliHz(),
                       MotorCtrl_SineGetCurrentAmplitudePermille(),
                       (unsigned long)MotorCtrl_SineGetPhaseStepQ16(),
                       (unsigned long)MotorCtrl_SineGetPhaseAccQ16(),
                       DRV8313_IsAwake(),
                       DRV8313_IsFaultActive());
            } else {
                printf("mode=sixstep run=%u step=%u tick=%u awake=%u fault=%u latched=%u\r\n",
                       MotorCtrl_IsRunning(),
                       MotorCtrl_GetCurrentStep(),
                       MotorCtrl_GetTickCounter(),
                       DRV8313_IsAwake(),
                       DRV8313_IsFaultActive(),
                       is_fault);
            }
        }
    }
}

static void ControlTask_Callback(void)//TIM4任务
{
    static uint16_t led_cnt = 0U;

    led_cnt++;
    if (led_cnt >= 500U) {
        led_cnt = 0U;
        HAL_GPIO_TogglePin(Alive_GPIO_PORT, Alive_PIN);
    }

    if (DRV8313_IsFaultActive() != 0U) {
        is_fault = 1U;
    }

    MotorCtrl_Task();
}

static void HeartbeatLed_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    gpio.Pin = Alive_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(Alive_GPIO_PORT, &gpio);

    HAL_GPIO_WritePin(Alive_GPIO_PORT, Alive_PIN, GPIO_PIN_RESET);
}

static void TaskTimer_Init(void)//TIM4定时器初始化
{
    TIM4_TIMER_CLK_ENABLE();

    s_task_tim.Instance = TIM4_TIMER_INSTANCE;
    s_task_tim.Init.Prescaler = TIM4_TIMER_PRESCALER;
    s_task_tim.Init.CounterMode = TIM_COUNTERMODE_UP;
    s_task_tim.Init.Period = TIM4_TIMER_PERIOD;
    s_task_tim.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    s_task_tim.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_Base_Init(&s_task_tim) != HAL_OK) {
        Error_Handler();
    }

    HAL_NVIC_SetPriority(TIM4_TIMER_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(TIM4_TIMER_IRQn);
}

static void TaskTimer_Start(void)//TIM4定时器开启
{
    if (HAL_TIM_Base_Start_IT(&s_task_tim) != HAL_OK) {
        Error_Handler();
    }
}

void TIM4_IRQHandler(void)//绑定回调TIM4 
{
    HAL_TIM_IRQHandler(&s_task_tim);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)//回调TIM4 
{
    if (htim->Instance == TIM4_TIMER_INSTANCE) {
        ControlTask_Callback();
    }
}

static void SystemClock_Config(void)
{
    RCC_ClkInitTypeDef RCC_ClkInitStruct;
    RCC_OscInitTypeDef RCC_OscInitStruct;

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = 0x10;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = 16;
    RCC_OscInitStruct.PLL.PLLN = 400;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
    RCC_OscInitStruct.PLL.PLLQ = 7;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_HCLK |
                                  RCC_CLOCKTYPE_PCLK1 |
                                  RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK) {
        Error_Handler();
    }
}

static void Error_Handler(void)
{
    while (1) {
        HAL_GPIO_TogglePin(Alive_GPIO_PORT, Alive_PIN);
        HAL_Delay(100);
    }
}
