#include "motor_sine_openloop.h"
#include "drv8313.h"
#include "pwm_out.h"

/*
 * 正弦开环输出模块。
 *
 * 当前实现的核心思路很直接：
 * 1. 用 96 点正弦表表示一个电角度周期；
 * 2. 用 Q16 定点累加器 phase_acc_q16 记录当前相位；
 * 3. 每次 20kHz PWM 更新中断到来时，按 phase_step_q16 推进一步；
 * 4. 取 U/V/W 三个相位点（彼此相差 120 度），换算成 0~1000 占空比；
 * 5. 再交给 drv8313 / pwm_out 输出到 TIM1 的三个通道。
 */

typedef struct
{
    uint8_t running;                    /* 1 表示 PWM 更新中断应该继续推进相位。 */
    uint8_t reverse;                    /* 1 表示相位反方向累加，用来实现反转。 */
    uint32_t phase_acc_q16;             /* 当前相位累加器：高 16 位是表索引，低 16 位保留小数。 */
    uint32_t phase_step_q16;            /* 每个 PWM 更新周期相位前进多少，单位同样是 Q16。 */
    uint32_t current_freq_mhz;          /* 当前实际输出频率，单位 mHz，10ms 任务里渐变。 */
    uint32_t target_freq_mhz;           /* 目标频率，10ms 任务朝这个值缓慢逼近。 */
    uint32_t freq_ramp_mhz_per_10ms;    /* 每个 10ms 允许改多少频率，避免瞬间跳变。 */
    uint16_t current_amp_permille;      /* 当前调制幅值，0~1000 表示 0%~100%。 */
    uint16_t target_amp_permille;       /* 目标幅值。 */
    uint16_t amp_ramp_permille_per_10ms;/* 每个 10ms 的幅值爬升/下降步长。 */
} motor_sine_openloop_t;

/*
 * 96 点正弦表，幅值范围是 -1000 ~ 1000。
 * 这样后面和 amp_permille 相乘后，再叠加到 PWM_DUTY_NEUTRAL=500，
 * 就能自然映射到 0~1000 的占空比范围。
 */
static const int16_t k_sine_table_96[MOTOR_SINE_TABLE_SIZE] = {
       0,   65,  131,  195,  259,  321,  383,  442,
     500,  556,  609,  659,  707,  752,  793,  831,
     866,  897,  924,  947,  966,  981,  991,  998,
    1000,  998,  991,  981,  966,  947,  924,  897,
     866,  831,  793,  752,  707,  659,  609,  556,
     500,  442,  383,  321,  259,  195,  131,   65,
       0,  -65, -131, -195, -259, -321, -383, -442,
    -500, -556, -609, -659, -707, -752, -793, -831,
    -866, -897, -924, -947, -966, -981, -991, -998,
   -1000, -998, -991, -981, -966, -947, -924, -897,
    -866, -831, -793, -752, -707, -659, -609, -556,
    -500, -442, -383, -321, -259, -195, -131,  -65
};

static motor_sine_openloop_t s_sine;

static uint16_t MotorSineOpenLoop_ClampAmp(uint16_t amp_permille)
{
    if (amp_permille > 1000U) {
        return 1000U;
    }

    return amp_permille;
}

static uint32_t MotorSineOpenLoop_ClampIndex(uint32_t idx)
{
    /* 当前表大小固定为 96，这里统一做环形回绕。 */
    while (idx >= MOTOR_SINE_TABLE_SIZE) {
        idx -= MOTOR_SINE_TABLE_SIZE;
    }

    return idx;
}

static uint32_t MotorSineOpenLoop_FreqToPhaseStepQ16(uint32_t freq_mhz)
{
    uint64_t numerator;
    uint64_t denominator;

    /*
     * 把“目标电频率”换算成“每次 20kHz 更新中断相位该前进多少”。
     *
     * 分子：频率 * 一整圈对应的表长度 * Q16 缩放
     * 分母：PWM 更新频率 * 1000（因为输入频率单位是 mHz）
     */
    numerator = (uint64_t)freq_mhz * (uint64_t)MOTOR_SINE_TABLE_SIZE * 65536ULL;
    denominator = (uint64_t)pwm_out_get_update_rate_hz() * 1000ULL;

    return (uint32_t)(numerator / denominator);
}

static uint32_t MotorSineOpenLoop_RampU32(uint32_t current, uint32_t target, uint32_t step)
{
    if (current < target) {
        current += step;
        if (current > target) {
            current = target;
        }
        return current;
    }

    if (current > target) {
        if (current > step) {
            current -= step;
        } else {
            current = 0U;
        }

        if (current < target) {
            current = target;
        }
    }

    return current;
}

static uint16_t MotorSineOpenLoop_RampU16(uint16_t current, uint16_t target, uint16_t step)
{
    if (current < target) {
        current = (uint16_t)(current + step);
        if (current > target) {
            current = target;
        }
        return current;
    }

    if (current > target) {
        if (current > step) {
            current = (uint16_t)(current - step);
        } else {
            current = 0U;
        }

        if (current < target) {
            current = target;
        }
    }

    return current;
}

static uint16_t MotorSineOpenLoop_SineToDuty(int16_t sine_permille, uint16_t amp_permille)
{
    int32_t duty;
    int32_t modulation;

    /*
     * sine_permille 范围是 -1000~1000，amp_permille 范围是 0~1000。
     * 乘完再除以 2000，得到 -500~500 的调制量，最后叠加到中点 500。
     * 这样：
     * - 幅值为 1000 时，占空比范围接近 0~1000；
     * - 幅值为 0 时，三相都会落在 500，也就是中点电平。
     */
    modulation = ((int32_t)sine_permille * (int32_t)amp_permille) / 2000;
    duty = (int32_t)PWM_DUTY_NEUTRAL + modulation;

    if (duty < (int32_t)PWM_DUTY_MIN) {
        duty = PWM_DUTY_MIN;
    } else if (duty > (int32_t)PWM_DUTY_MAX) {
        duty = PWM_DUTY_MAX;
    }

    return (uint16_t)duty;
}

static void MotorSineOpenLoop_UpdatePwm(void)
{
    uint32_t idx_u;
    uint32_t idx_v;
    uint32_t idx_w;

    /*
     * phase_acc_q16 的高 16 位直接当作整点索引。
     * V/W 相分别在 U 相基础上平移 120 度和 240 度。
     * 因为表长是 96，所以 120 度 = 96 / 3 = 32 点。
     */
    idx_u = s_sine.phase_acc_q16 >> 16;
    idx_v = MotorSineOpenLoop_ClampIndex(idx_u + MOTOR_SINE_PHASE_SHIFT_120_DEG);
    idx_w = MotorSineOpenLoop_ClampIndex(idx_u + (2U * MOTOR_SINE_PHASE_SHIFT_120_DEG));

    DRV8313_SetPhasePwmDuty(DRV_PHASE_U,
                            MotorSineOpenLoop_SineToDuty(k_sine_table_96[idx_u],
                                                         s_sine.current_amp_permille));
    DRV8313_SetPhasePwmDuty(DRV_PHASE_V,
                            MotorSineOpenLoop_SineToDuty(k_sine_table_96[idx_v],
                                                         s_sine.current_amp_permille));
    DRV8313_SetPhasePwmDuty(DRV_PHASE_W,
                            MotorSineOpenLoop_SineToDuty(k_sine_table_96[idx_w],
                                                         s_sine.current_amp_permille));
}

static void MotorSineOpenLoop_HighFreqTask(void)
{
    const uint32_t full_scale_q16 = ((uint32_t)MOTOR_SINE_TABLE_SIZE << 16);

    /*
     * 这是整个正弦模式最关键的高频任务。
     * 它挂在 TIM1 的更新中断上，理论频率是 20kHz。
     * 只有放在这里，相位推进和 PWM 刷新才足够平滑；
     * 如果挪到 10ms 任务里，波形会粗到几乎只剩台阶。
     */
    if (s_sine.running == 0U) {
        return;
    }

    if (s_sine.reverse != 0U) {
        if (s_sine.phase_acc_q16 >= s_sine.phase_step_q16) {
            s_sine.phase_acc_q16 -= s_sine.phase_step_q16;
        } else {
            s_sine.phase_acc_q16 = full_scale_q16 - (s_sine.phase_step_q16 - s_sine.phase_acc_q16);
        }
    } else {
        s_sine.phase_acc_q16 += s_sine.phase_step_q16;
        while (s_sine.phase_acc_q16 >= full_scale_q16) {
            s_sine.phase_acc_q16 -= full_scale_q16;
        }
    }

    /* 相位更新完成后，立刻把新的 U/V/W 三相占空比写到输出层。 */
    MotorSineOpenLoop_UpdatePwm();
}

void MotorSineOpenLoop_Init(void)
{
    s_sine.running = 0U;
    s_sine.reverse = 0U;
    s_sine.phase_acc_q16 = 0U;
    s_sine.phase_step_q16 = 0U;
    s_sine.current_freq_mhz = 0U;
    s_sine.target_freq_mhz = MOTOR_SINE_DEFAULT_TARGET_FREQ_MHZ;
    s_sine.freq_ramp_mhz_per_10ms = MOTOR_SINE_DEFAULT_FREQ_RAMP_MHZ_PER_10MS;
    s_sine.current_amp_permille = 0U;
    s_sine.target_amp_permille = MOTOR_SINE_DEFAULT_TARGET_AMP_PERMILLE;
    s_sine.amp_ramp_permille_per_10ms = MOTOR_SINE_DEFAULT_AMP_RAMP_PER_10MS;

    /* 高频任务只注册一次，真正启停靠 pwm_out_enable_update_irq 控制。 */
    pwm_out_register_update_callback(MotorSineOpenLoop_HighFreqTask);
}

HAL_StatusTypeDef MotorSineOpenLoop_Start(void)
{
    /* 每次启动都从 0 频率、0 幅值、0 相位重新拉起。 */
    s_sine.running = 0U;
    s_sine.phase_acc_q16 = 0U;
    s_sine.phase_step_q16 = 0U;
    s_sine.current_freq_mhz = 0U;
    s_sine.current_amp_permille = 0U;

    if (DRV8313_Wakeup() != HAL_OK) {
        return HAL_ERROR;
    }

    /*
     * 启动瞬间先把三相都打到 500 的中点占空比，再打开输出。
     * 这样不会一上电就突然给出大幅值脉冲。
     */
    DRV8313_SetPhasePwmDuty(DRV_PHASE_U, PWM_DUTY_NEUTRAL);
    DRV8313_SetPhasePwmDuty(DRV_PHASE_V, PWM_DUTY_NEUTRAL);
    DRV8313_SetPhasePwmDuty(DRV_PHASE_W, PWM_DUTY_NEUTRAL);
    DRV8313_EnableAllOutputs();

    s_sine.running = 1U;
    pwm_out_enable_update_irq(1U);

    return HAL_OK;
}

void MotorSineOpenLoop_Stop(void)
{
    /* 先关更新中断，再清状态，避免停机过程中相位还在继续推进。 */
    pwm_out_enable_update_irq(0U);
    s_sine.running = 0U;
    s_sine.phase_step_q16 = 0U;
    s_sine.current_freq_mhz = 0U;
    s_sine.current_amp_permille = 0U;
}

void MotorSineOpenLoop_Task10ms(void)
{
    /*
     * 10ms 任务只做“慢变量”更新：
     * - current_freq_mhz 朝 target_freq_mhz 渐变
     * - current_amp_permille 朝 target_amp_permille 渐变
     * - 再把当前频率换算成 phase_step_q16
     *
     * 真正每个 PWM 周期的相位推进并不在这里做。
     */
    s_sine.current_freq_mhz = MotorSineOpenLoop_RampU32(s_sine.current_freq_mhz,
                                                        s_sine.target_freq_mhz,
                                                        s_sine.freq_ramp_mhz_per_10ms);
    s_sine.current_amp_permille = MotorSineOpenLoop_RampU16(s_sine.current_amp_permille,
                                                            s_sine.target_amp_permille,
                                                            s_sine.amp_ramp_permille_per_10ms);
    s_sine.phase_step_q16 = MotorSineOpenLoop_FreqToPhaseStepQ16(s_sine.current_freq_mhz);
}

void MotorSineOpenLoop_SetReverse(uint8_t reverse)
{
    s_sine.reverse = (reverse != 0U) ? 1U : 0U;
}

void MotorSineOpenLoop_SetTargetFreqMilliHz(uint32_t freq_mhz)
{
    s_sine.target_freq_mhz = freq_mhz;
}

void MotorSineOpenLoop_SetFreqRampMilliHzPer10ms(uint32_t ramp_mhz)
{
    s_sine.freq_ramp_mhz_per_10ms = (ramp_mhz == 0U) ? 1U : ramp_mhz;
}

void MotorSineOpenLoop_SetTargetAmplitudePermille(uint16_t amp_permille)
{
    /* 幅值统一限制在 0~1000，避免后面 duty 计算越界。 */
    s_sine.target_amp_permille = MotorSineOpenLoop_ClampAmp(amp_permille);
}

void MotorSineOpenLoop_SetAmpRampPermillePer10ms(uint16_t ramp_permille)
{
    s_sine.amp_ramp_permille_per_10ms = (ramp_permille == 0U) ? 1U : ramp_permille;
}

uint8_t MotorSineOpenLoop_IsRunning(void)
{
    return s_sine.running;
}

uint8_t MotorSineOpenLoop_IsReverse(void)
{
    return s_sine.reverse;
}

uint32_t MotorSineOpenLoop_GetPhaseAccQ16(void)
{
    return s_sine.phase_acc_q16;
}

uint32_t MotorSineOpenLoop_GetPhaseStepQ16(void)
{
    return s_sine.phase_step_q16;
}

uint32_t MotorSineOpenLoop_GetCurrentFreqMilliHz(void)
{
    return s_sine.current_freq_mhz;
}

uint32_t MotorSineOpenLoop_GetTargetFreqMilliHz(void)
{
    return s_sine.target_freq_mhz;
}

uint16_t MotorSineOpenLoop_GetCurrentAmplitudePermille(void)
{
    return s_sine.current_amp_permille;
}

uint16_t MotorSineOpenLoop_GetTargetAmplitudePermille(void)
{
    return s_sine.target_amp_permille;
}
