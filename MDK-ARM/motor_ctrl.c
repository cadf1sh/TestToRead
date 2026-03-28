#include "motor_ctrl.h"
#include "drv8313.h"
#include "motor_sine_openloop.h"

/*
 * 电机控制调度层。
 *
 * 这个文件站在“模式管理”的位置：
 * - six-step 模式时，它负责按固定节拍推进换相表；
 * - sine open-loop 模式时，它把启动、停止、10ms 任务转交给 motor_sine_openloop.c；
 * - 更底层的驱动器唤醒、相线输出则交给 drv8313.c。
 */

typedef struct
{
    uint8_t current_step; /* 当前已经输出到驱动器的六步序号，范围 0~5。 */
    uint8_t start_step;   /* 每次重新启动时，从哪个换相步开始。 */
    uint16_t tick_count;  /* 当前步已经累计了多少个 10ms 周期。 */
    uint16_t step_ticks;  /* 每隔多少个 10ms 周期推进到下一步。 */
} motor_sixstep_state_t;

typedef struct
{
    uint8_t running;               /* 电机控制层是否处于“已启动”状态。 */
    motor_mode_t mode;             /* 当前工作模式：六步换相 or 正弦开环。 */
    motor_direction_t direction;   /* 当前方向，影响 six-step 下一步和正弦相位推进方向。 */
    motor_sixstep_state_t sixstep; /* six-step 模式自己的运行上下文。 */
} motor_ctrl_t;

/*
 * 6-step table (U, V, W):
 * STEP1: U+, V-, W0
 * STEP2: U0, V-, W+
 * STEP3: U-, V0, W+
 * STEP4: U-, V+, W0
 * STEP5: U0, V+, W-
 * STEP6: U+, V0, W-
 */
static const drv_phase_state_t k_six_step_table[6][3] = {
    {DRV_PHASE_POSITIVE, DRV_PHASE_NEGATIVE, DRV_PHASE_OFF},
    {DRV_PHASE_OFF,      DRV_PHASE_NEGATIVE, DRV_PHASE_POSITIVE},
    {DRV_PHASE_NEGATIVE, DRV_PHASE_OFF,      DRV_PHASE_POSITIVE},
    {DRV_PHASE_NEGATIVE, DRV_PHASE_POSITIVE, DRV_PHASE_OFF},
    {DRV_PHASE_OFF,      DRV_PHASE_POSITIVE, DRV_PHASE_NEGATIVE},
    {DRV_PHASE_POSITIVE, DRV_PHASE_OFF,      DRV_PHASE_NEGATIVE}
};

/* 整个控制层的单例状态都收在这里。 */
static motor_ctrl_t s_motor;

static void MotorCtrl_ApplySixStep(uint8_t step)
{
    /* 当前步号先查表，再把 U/V/W 三相状态分别下发给驱动层。 */
    const drv_phase_state_t *commutation = k_six_step_table[step % 6U];

    DRV8313_SetPhaseState(DRV_PHASE_U, commutation[0]);
    DRV8313_SetPhaseState(DRV_PHASE_V, commutation[1]);
    DRV8313_SetPhaseState(DRV_PHASE_W, commutation[2]);
}

static uint8_t MotorCtrl_GetNextSixStep(uint8_t step)
{
    /* 反转时等价于在 6 个状态里反方向循环。 */
    if (s_motor.direction == MOTOR_DIR_REVERSE) {
        return (uint8_t)((step + 5U) % 6U);
    }

    return (uint8_t)((step + 1U) % 6U);
}

static HAL_StatusTypeDef MotorCtrl_StartSixStep(void)
{
    /* 每次启动都回到设定的起始步，避免沿用上次停止时的残留状态。 */
    s_motor.sixstep.current_step = s_motor.sixstep.start_step;
    s_motor.sixstep.tick_count = 0U;

    /* 先把驱动器从 sleep/reset 拉起来，成功后再真正上相。 */
    if (DRV8313_Wakeup() != HAL_OK) {
        DRV8313_EnterSafeState();
        return HAL_ERROR;
    }

    MotorCtrl_ApplySixStep(s_motor.sixstep.current_step);
    return HAL_OK;
}

static HAL_StatusTypeDef MotorCtrl_StartSineOpenLoop(void)
{
    /* 正弦开环模块内部也维护方向，所以这里要同步过去。 */
    MotorSineOpenLoop_SetReverse((s_motor.direction == MOTOR_DIR_REVERSE) ? 1U : 0U);
    return MotorSineOpenLoop_Start();
}

static void MotorCtrl_TaskSixStep10ms(void)
{
    /* six-step 模式的步进节拍完全由 10ms 低频任务决定。 */
    s_motor.sixstep.tick_count++;
    if (s_motor.sixstep.tick_count < s_motor.sixstep.step_ticks) {
        return;
    }

    s_motor.sixstep.tick_count = 0U;
    s_motor.sixstep.current_step = MotorCtrl_GetNextSixStep(s_motor.sixstep.current_step);
    MotorCtrl_ApplySixStep(s_motor.sixstep.current_step);
}

void MotorCtrl_Init(void)
{
    /* 这里做的是“控制层上电缺省值”初始化，不等于真正启动输出。 */
    s_motor.running = 0U;
    s_motor.mode = MOTOR_MODE_SIXSTEP;
    s_motor.direction = MOTOR_DIR_FORWARD;
    s_motor.sixstep.start_step = 0U;
    s_motor.sixstep.current_step = 0U;
    s_motor.sixstep.tick_count = 0U;
    s_motor.sixstep.step_ticks = MOTOR_OPEN_LOOP_STEP_TICKS_DEFAULT;

    MotorSineOpenLoop_Init();
    /* 初始化完成后先把三相全部关掉，避免上电瞬间误导通。 */
    DRV8313_AllPhaseOff();
}

HAL_StatusTypeDef MotorCtrl_Start(void)
{
    HAL_StatusTypeDef status;

    /* 启动前统一先停一次，把上次运行残留状态清干净。 */
    MotorCtrl_Stop();

    if (s_motor.mode == MOTOR_MODE_SINE_OPENLOOP) {
        status = MotorCtrl_StartSineOpenLoop();
    } else {
        status = MotorCtrl_StartSixStep();
    }

    if (status != HAL_OK) {
        return HAL_ERROR;
    }

    s_motor.running = 1U;
    return HAL_OK;
}

void MotorCtrl_Stop(void)
{
    /* 停止时不仅要记 running=0，还要让驱动器回安全态。 */
    s_motor.running = 0U;
    s_motor.sixstep.current_step = s_motor.sixstep.start_step;
    s_motor.sixstep.tick_count = 0U;

    MotorSineOpenLoop_Stop();
    DRV8313_EnterSafeState();
}

void MotorCtrl_Task10ms(void)
{
    /* 控制层的 10ms 任务入口由 main.c 的定时回调统一调用。 */
    if (s_motor.running == 0U) {
        return;
    }

    /* 低频任务里先做故障拦截，发现 fault 立刻停机。 */
    if (DRV8313_IsFaultActive() != 0U) {
        MotorCtrl_Stop();
        return;
    }

    /* 两种模式在这里正式分流。 */
    if (s_motor.mode == MOTOR_MODE_SINE_OPENLOOP) {
        MotorSineOpenLoop_Task10ms();
        return;
    }

    MotorCtrl_TaskSixStep10ms();
}

void MotorCtrl_SetMode(motor_mode_t mode)
{
    if (mode > MOTOR_MODE_SINE_OPENLOOP) {
        return;
    }

    /* 运行中切模式，先停机，避免新旧模式同时驱动同一组输出。 */
    if (s_motor.running != 0U) {
        MotorCtrl_Stop();
    }

    s_motor.mode = mode;
}

motor_mode_t MotorCtrl_GetMode(void)
{
    return s_motor.mode;
}

void MotorCtrl_SetDirection(motor_direction_t dir)
{
    /* 当前工程里 direction 同时服务于 six-step 和正弦开环。 */
    s_motor.direction = dir;
    MotorSineOpenLoop_SetReverse((dir == MOTOR_DIR_REVERSE) ? 1U : 0U);
}

motor_direction_t MotorCtrl_GetDirection(void)
{
    return s_motor.direction;
}

void MotorCtrl_SetSixStepStepTicks(uint16_t ticks)
{
    /* six-step 至少要保证 1 个 10ms 周期，不允许为 0。 */
    s_motor.sixstep.step_ticks = (ticks == 0U) ? 1U : ticks;
}

uint16_t MotorCtrl_GetSixStepStepTicks(void)
{
    return s_motor.sixstep.step_ticks;
}

void MotorCtrl_SetStartStep(uint8_t step_0_to_5)
{
    /* 输入统一压回 0~5，避免上层传入越界值。 */
    s_motor.sixstep.start_step = (uint8_t)(step_0_to_5 % 6U);

    /* 仅在停机状态下同步 current_step，运行中不强改当前换相。 */
    if (s_motor.running == 0U) {
        s_motor.sixstep.current_step = s_motor.sixstep.start_step;
    }
}

uint8_t MotorCtrl_GetStartStep(void)
{
    return s_motor.sixstep.start_step;
}

uint8_t MotorCtrl_GetCurrentStep(void)
{
    return s_motor.sixstep.current_step;
}

uint16_t MotorCtrl_GetTickCounter(void)
{
    return s_motor.sixstep.tick_count;
}

uint8_t MotorCtrl_IsRunning(void)
{
    return s_motor.running;
}

void MotorCtrl_SineSetTargetFreqMilliHz(uint32_t freq_mhz)
{
    /* 正弦模式的细节参数全部转发给专用模块处理。 */
    MotorSineOpenLoop_SetTargetFreqMilliHz(freq_mhz);
}

void MotorCtrl_SineSetFreqRampMilliHzPer10ms(uint32_t ramp_mhz)
{
    MotorSineOpenLoop_SetFreqRampMilliHzPer10ms(ramp_mhz);
}

void MotorCtrl_SineSetAmplitudePermille(uint16_t amp_permille)
{
    MotorSineOpenLoop_SetTargetAmplitudePermille(amp_permille);
}

void MotorCtrl_SineSetAmpRampPermillePer10ms(uint16_t ramp_permille)
{
    MotorSineOpenLoop_SetAmpRampPermillePer10ms(ramp_permille);
}

uint32_t MotorCtrl_SineGetCurrentFreqMilliHz(void)
{
    return MotorSineOpenLoop_GetCurrentFreqMilliHz();
}

uint32_t MotorCtrl_SineGetPhaseAccQ16(void)
{
    return MotorSineOpenLoop_GetPhaseAccQ16();
}

uint32_t MotorCtrl_SineGetPhaseStepQ16(void)
{
    return MotorSineOpenLoop_GetPhaseStepQ16();
}

uint16_t MotorCtrl_SineGetCurrentAmplitudePermille(void)
{
    return MotorSineOpenLoop_GetCurrentAmplitudePermille();
}
