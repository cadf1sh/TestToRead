#include "app_config.h"
#include "drv8313.h"
#include "motor_ctrl.h"
#include "my_timer.h"
#include "my_uart.h"
#include <stdio.h>

/*
 * 应用层主入口。
 *
 * 这个文件不负责具体的换相表、正弦查表或 PWM 映射，
 * 它做的是把整套控制链真正“跑起来”：
 * 1. 完成 HAL、系统时钟、串口、心跳灯初始化；
 * 2. 初始化 DRV8313 驱动层和电机控制层；
 * 3. 根据 app_config.h 里的启动模式，给控制层写入初始参数；
 * 4. 启动 10ms 低频任务；
 * 5. 在 while(1) 里做状态打印，方便上板观察当前运行情况。
 */

static void SystemClock_Config(void);
static void Error_Handler(void);
static void Task_10msCallback(void);
static void HeartbeatLed_Init(void);
static void MotorCtrl_ApplyStartupConfig(void);

/* 一旦检测到驱动器故障，就把这个标志锁存下来，供主循环日志观察。 */
static volatile uint8_t g_fault_latched = 0U;

int main(void)
{
    /* HAL_Init 会初始化 SysTick 和 HAL 内部状态，必须最先做。 */
    HAL_Init();
    /* 先把心跳灯 GPIO 配出来，后面即使时钟初始化失败也能闪灯报错。 */
    HeartbeatLed_Init();
    SystemClock_Config();

    /* 串口仅用于打印调试信息，不参与控制闭环。 */
    my_Uart_Init();
    printf("boot\r\n");

    /* 先准备底层驱动，再初始化上层控制器，顺序不能反。 */
    DRV8313_Init();
    MotorCtrl_Init();
    /* 把默认模式、方向、目标频率/幅值等启动参数写入控制层。 */
    MotorCtrl_ApplyStartupConfig();

    /* 给驱动器上电稳定留一点余量。 */
    HAL_Delay(5);

    /* 10ms 定时任务负责低频调度：LED 心跳、故障巡检、模式低频更新。 */
    my_Timer10ms_Init();
    my_Timer10ms_RegisterCallback(Task_10msCallback);
    my_Timer10ms_Start();

    /* 真正启动电机输出。启动失败时不再继续重试，只记故障。 */
    if (MotorCtrl_Start() != HAL_OK) {
        g_fault_latched = 1U;
        printf("start failed fault=%u\r\n", DRV8313_IsFaultActive());
    }

    {
        uint32_t last_log_ms = 0U;

        while (1) {
            uint32_t now = HAL_GetTick();

            if ((now - last_log_ms) >= 200U) {
                last_log_ms = now;

                /* 正弦模式下更关心频率、幅值和相位累加器。 */
                if (MotorCtrl_GetMode() == MOTOR_MODE_SINE_OPENLOOP) {
                    printf("mode=sine run=%u freq=%u amp=%u phase_step=%u phase_acc=%u awake=%u fault=%u\r\n",
                           MotorCtrl_IsRunning(),
                           MotorCtrl_SineGetCurrentFreqMilliHz(),
                           MotorCtrl_SineGetCurrentAmplitudePermille(),
                           MotorCtrl_SineGetPhaseStepQ16(),
                           MotorCtrl_SineGetPhaseAccQ16(),
                           DRV8313_IsAwake(),
                           DRV8313_IsFaultActive());
                } else {
                    /*
                     * 当前源码这里原本的 printf 已经残缺，只剩下参数列表。
                     * 从保留下来的参数顺序可以确定，这里是在打印 six-step
                     * 模式的运行状态。下面按现有参数语义恢复日志文本，
                     * 不改变控制行为，只把源码修回可读、可编译状态。
                     */
                    printf("mode=sixstep run=%u step=%u tick=%u awake=%u fault=%u latched=%u\r\n",
                           MotorCtrl_IsRunning(),
                           MotorCtrl_GetCurrentStep(),
                           MotorCtrl_GetTickCounter(),
                           DRV8313_IsAwake(),
                           DRV8313_IsFaultActive(),
                           g_fault_latched);
                }
            }
        }
    }
}

static void MotorCtrl_ApplyStartupConfig(void)
{
    /*
     * 启动参数只在上电初始化时统一下发一次。
     * 后续真正的执行节奏由 10ms 任务和 PWM 更新中断驱动。
     */
    if (APP_STARTUP_MOTOR_MODE == APP_STARTUP_MODE_SINE_OPENLOOP) {
        MotorCtrl_SetMode(MOTOR_MODE_SINE_OPENLOOP);
        MotorCtrl_SetDirection(MOTOR_DIR_FORWARD);
        MotorCtrl_SineSetTargetFreqMilliHz(MOTOR_SINE_DEFAULT_TARGET_FREQ_MHZ);
        MotorCtrl_SineSetFreqRampMilliHzPer10ms(MOTOR_SINE_DEFAULT_FREQ_RAMP_MHZ_PER_10MS);
        MotorCtrl_SineSetAmplitudePermille(MOTOR_SINE_DEFAULT_TARGET_AMP_PERMILLE);
        MotorCtrl_SineSetAmpRampPermillePer10ms(MOTOR_SINE_DEFAULT_AMP_RAMP_PER_10MS);
        printf("startup mode=sine_openloop\r\n");
        return;
    }

    MotorCtrl_SetMode(MOTOR_MODE_SIXSTEP);
    MotorCtrl_SetDirection(MOTOR_DIR_FORWARD);
    MotorCtrl_SetSixStepStepTicks(MOTOR_OPEN_LOOP_STEP_TICKS_DEFAULT);
    printf("startup mode=sixstep\r\n");
}

static void Task_10msCallback(void)
{
    /* 10ms * 50 = 500ms，心跳灯每 0.5s 翻转一次。 */
    static uint16_t led_cnt = 0U;

    led_cnt++;
    if (led_cnt >= 50U) {
        led_cnt = 0U;
        HAL_GPIO_TogglePin(APP_HEARTBEAT_GPIO_PORT, APP_HEARTBEAT_PIN);
    }

    /* 故障脚一旦有效，先把状态锁存下来，便于串口日志追踪。 */
    if (DRV8313_IsFaultActive() != 0U) {
        g_fault_latched = 1U;
    }

    /* 这里是整个工程的低频任务入口。 */
    MotorCtrl_Task10ms();
}

static void HeartbeatLed_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    gpio.Pin = APP_HEARTBEAT_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(APP_HEARTBEAT_GPIO_PORT, &gpio);

    HAL_GPIO_WritePin(APP_HEARTBEAT_GPIO_PORT, APP_HEARTBEAT_PIN, GPIO_PIN_RESET);
}

static void SystemClock_Config(void)
{
    RCC_ClkInitTypeDef RCC_ClkInitStruct;
    RCC_OscInitTypeDef RCC_OscInitStruct;

    /* 这里把系统时钟配置到 100MHz，后面的 10ms 定时器和 20kHz PWM 都以此为基准。 */
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
    /* 出错后只保留最基础的闪灯行为，避免在不确定状态下继续驱动电机。 */
    while (1) {
        HAL_GPIO_TogglePin(APP_HEARTBEAT_GPIO_PORT, APP_HEARTBEAT_PIN);
        HAL_Delay(100);
    }
}
