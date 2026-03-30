#include "motor_ctrl.h"
#include "drv8313.h"
#include "pwm_out.h"
uint16_t Sine_Udate_hz  =240;//微步换相更新频率

typedef struct
{
    uint8_t current_step;//六步换相状态号
    uint16_t tick_count;//时间标志位。当前步已经经过了多少个 1ms tick。
    uint16_t step_ticks;//速度标志位。每隔多少tick换下一步。
} motor_sixstep_state_t;

typedef struct
{
    uint32_t phase_acc;//微步细分相位累加器。
    uint32_t phase_step;//一次推进多少相位
    uint32_t current_freq;//当前频率
    uint32_t target_freq;//目标频率
    uint32_t freq_ramp;//频率上升
    uint16_t current_amp;//当前幅值
    uint16_t target_amp;//目标幅值
    uint16_t amp_ramp;//幅值上升
} SINE_state_t;

typedef struct
{
    uint8_t running;//运行状态
    motor_mode_t mode;//控制模式
    motor_direction_t direction;//运动方向
    motor_sixstep_state_t sixstep;//六步状态
    SINE_state_t sine;//微步状态
} motor_ctrl;

static const drv_phase_state sixstep_state_table[6][3] = {// UVW 的三相状态
    {DRV_PHASE_POSITIVE, DRV_PHASE_NEGATIVE, DRV_PHASE_OFF},
    {DRV_PHASE_OFF,      DRV_PHASE_NEGATIVE, DRV_PHASE_POSITIVE},
    {DRV_PHASE_NEGATIVE, DRV_PHASE_OFF,      DRV_PHASE_POSITIVE},
    {DRV_PHASE_NEGATIVE, DRV_PHASE_POSITIVE, DRV_PHASE_OFF},
    {DRV_PHASE_OFF,      DRV_PHASE_POSITIVE, DRV_PHASE_NEGATIVE},
    {DRV_PHASE_POSITIVE, DRV_PHASE_OFF,      DRV_PHASE_NEGATIVE}
};

static const int16_t sine_table_16[SINE_TABLE_SIZE] = {//16细分表
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

static motor_ctrl my_motor;

static void MotorCtrl_ApplySixStep(uint8_t step)//查表
{
    const drv_phase_state *commutation = sixstep_state_table[step % 6U];

    DRV8313_SetPhaseState(DRV_PHASE_U, commutation[0]);
    DRV8313_SetPhaseState(DRV_PHASE_V, commutation[1]);
    DRV8313_SetPhaseState(DRV_PHASE_W, commutation[2]);
}

static uint16_t MotorCtrl_ClampAmp(uint16_t amp_permille)
{
    if (amp_permille > 1000U) {
        return 1000U;
    }

    return amp_permille;
}

static uint32_t MotorCtrl_GetFreq(uint32_t current, uint32_t target, uint32_t step)
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

static uint16_t MotorCtrl_GetAmp(uint16_t current, uint16_t target, uint16_t step)
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


static uint16_t MotorCtrl_SineToDuty(int16_t sine_permille, uint16_t amp_permille)//查表输出转换为占空比
{
    int32_t duty;

    duty = (int32_t)PWM_DUTY_MID +
           (((int32_t)sine_permille * (int32_t)amp_permille) / 2000);

    if (duty < (int32_t)PWM_DUTY_MIN) {
        duty = PWM_DUTY_MIN;
    } else if (duty > (int32_t)PWM_DUTY_MAX) {
        duty = PWM_DUTY_MAX;
    }

    return (uint16_t)duty;
}

void MotorCtrl_Init(void)//运动初始化
{
    my_motor.running = 0U;
    my_motor.mode = MOTOR_MODE_SINE_OPENLOOP;
    my_motor.direction = MOTOR_DIR_FORWARD;
    my_motor.sixstep.current_step = 0U;
    my_motor.sixstep.tick_count = 0U;
    my_motor.sixstep.step_ticks = MOTOR_OPEN_LOOP_STEP_TICKS_DEFAULT;

    my_motor.sine.phase_acc = 0U;
    my_motor.sine.phase_step = 0U;
    my_motor.sine.current_freq = 0U;
    my_motor.sine.target_freq = SINE_DEFAULT_target_freq;
    my_motor.sine.freq_ramp = SINE_DEFAULT_freq_ramp;
    my_motor.sine.current_amp = 0U;
    my_motor.sine.target_amp = SINE_DEFAULT_target_amp;
    my_motor.sine.amp_ramp = SINE_DEFAULT_AMP_RAMP_PER_TICK;

    DRV8313_AllPhaseOff();
}

HAL_StatusTypeDef MotorCtrl_Start(void)//电机开始控制
{
    MotorCtrl_Stop();

    if (my_motor.mode == MOTOR_MODE_SINE_OPENLOOP) {
        my_motor.sine.phase_acc = 0U;
        my_motor.sine.phase_step = 0U;
        my_motor.sine.current_freq = 0U;
        my_motor.sine.current_amp = 0U;

        if (DRV8313_Wakeup() != HAL_OK) {
            DRV8313_EnterSafeState();
            return HAL_ERROR;
        }

        DRV8313_SetPhasePwmDuty(DRV_PHASE_U, PWM_DUTY_MID);
        DRV8313_SetPhasePwmDuty(DRV_PHASE_V, PWM_DUTY_MID);
        DRV8313_SetPhasePwmDuty(DRV_PHASE_W, PWM_DUTY_MID);
        DRV8313_EnableAllOutputs();
        pwm_out_enable_update_irq(1);
    } else {
        my_motor.sixstep.current_step = 0U;
        my_motor.sixstep.tick_count = 0U;

        if (DRV8313_Wakeup() != HAL_OK) {
            DRV8313_EnterSafeState();
            return HAL_ERROR;
        }

        MotorCtrl_ApplySixStep(my_motor.sixstep.current_step);
    }

    my_motor.running = 1U;
    return HAL_OK;
}

void MotorCtrl_Stop(void)
{
    my_motor.running = 0U;
    my_motor.sixstep.current_step = 0U;
    my_motor.sixstep.tick_count = 0U;

    my_motor.sine.phase_acc = 0U;
    my_motor.sine.phase_step = 0U;
    my_motor.sine.current_freq = 0U;
    my_motor.sine.current_amp = 0U;

    pwm_out_enable_update_irq(0U);
    DRV8313_EnterSafeState();
}

void Motor_speed(uint16_t hz)
{
  my_motor.sine.phase_step = hz/64u*64u;
	return;
}

void MotorCtrl_Task(void)//电机任务安排
{
    if (my_motor.running == 0U) {
        return;
    }

    if (DRV8313_IsFaultActive() != 0U) {
        MotorCtrl_Stop();
        return;
    }

    if (my_motor.mode == MOTOR_MODE_SINE_OPENLOOP) {
        my_motor.sine.current_freq = MotorCtrl_GetFreq(my_motor.sine.current_freq,
                                                          my_motor.sine.target_freq,
                                                          my_motor.sine.freq_ramp);
        my_motor.sine.current_amp = MotorCtrl_GetAmp(my_motor.sine.current_amp,
                                                              my_motor.sine.target_amp,
                                                              my_motor.sine.amp_ramp);
        Motor_speed(65535);
        return;
    }

    my_motor.sixstep.tick_count++;
    if (my_motor.sixstep.tick_count < my_motor.sixstep.step_ticks) {
        return;
    }

    my_motor.sixstep.tick_count = 0U;

    if (my_motor.direction == MOTOR_DIR_REVERSE) {
        my_motor.sixstep.current_step = (uint8_t)((my_motor.sixstep.current_step + 5U) % 6U);
    } else {
        my_motor.sixstep.current_step = (uint8_t)((my_motor.sixstep.current_step + 1U) % 6U);
    }

    MotorCtrl_ApplySixStep(my_motor.sixstep.current_step);
}

void MotorCtrl_PwmUpdateHandler(void)//微步控制pwm更新
{
    uint32_t idx_u;
    uint32_t idx_v;
    uint32_t idx_w;
    const uint32_t full_scale_q16 = ((uint32_t)SINE_TABLE_SIZE << 16);

    if ((my_motor.running == 0U) || (my_motor.mode != MOTOR_MODE_SINE_OPENLOOP)) {
        return;
    }

    if (my_motor.direction == MOTOR_DIR_REVERSE) {
        if (my_motor.sine.phase_acc >= my_motor.sine.phase_step) {
            my_motor.sine.phase_acc -= my_motor.sine.phase_step;
        } else {
            my_motor.sine.phase_acc = full_scale_q16 -
                                         (my_motor.sine.phase_step - my_motor.sine.phase_acc);
        }
    } else {
        my_motor.sine.phase_acc += my_motor.sine.phase_step;
        while (my_motor.sine.phase_acc >= full_scale_q16) {
            my_motor.sine.phase_acc -= full_scale_q16;
        }
    }

    idx_u = my_motor.sine.phase_acc >> 16;
    idx_v = idx_u + SINE_PHASE_SHIFT_120_DEG;
    idx_w = idx_u + (2U * SINE_PHASE_SHIFT_120_DEG);

    if (idx_v >= SINE_TABLE_SIZE) {
        idx_v -= SINE_TABLE_SIZE;
    }

    if (idx_w >= SINE_TABLE_SIZE) {
        idx_w -= SINE_TABLE_SIZE;
    }

    DRV8313_SetPhasePwmDuty(DRV_PHASE_U,
                            MotorCtrl_SineToDuty(sine_table_16[idx_u],
                                                 my_motor.sine.current_amp));
    DRV8313_SetPhasePwmDuty(DRV_PHASE_V,
                            MotorCtrl_SineToDuty(sine_table_16[idx_v],
                                                 my_motor.sine.current_amp));
    DRV8313_SetPhasePwmDuty(DRV_PHASE_W,
                            MotorCtrl_SineToDuty(sine_table_16[idx_w],
                                                 my_motor.sine.current_amp));
}

void MotorCtrl_SetMode(motor_mode_t mode)//设置控制模式
{
    if (mode > MOTOR_MODE_SINE_OPENLOOP) {
        return;
    }

    if (my_motor.running != 0U) {
        MotorCtrl_Stop();
    }

    my_motor.mode = mode;
}

motor_mode_t MotorCtrl_GetMode(void)
{
    return my_motor.mode;
}

void MotorCtrl_SetDirection(motor_direction_t dir)
{
    my_motor.direction = dir;
}

void MotorCtrl_SetSixStepStepTicks(uint16_t ticks)
{
    my_motor.sixstep.step_ticks = (ticks == 0U) ? 1U : ticks;
}

uint8_t MotorCtrl_GetCurrentStep(void)
{
    return my_motor.sixstep.current_step;
}

uint16_t MotorCtrl_GetTickCounter(void)
{
    return my_motor.sixstep.tick_count;
}

uint8_t MotorCtrl_IsRunning(void)
{
    return my_motor.running;
}

void MotorCtrl_SineSetTargetFreqMilliHz(uint32_t freq_mhz)
{
    my_motor.sine.target_freq = freq_mhz;
}

void MotorCtrl_SineSetFreqRampMilliHzPerTick(uint32_t ramp_mhz)
{
    my_motor.sine.freq_ramp = (ramp_mhz == 0U) ? 1U : ramp_mhz;
}

void MotorCtrl_SineSetAmplitudePermille(uint16_t amp_permille)
{
    my_motor.sine.target_amp = MotorCtrl_ClampAmp(amp_permille);
}

void MotorCtrl_SineSetAmpRampPermillePerTick(uint16_t ramp_permille)
{
    my_motor.sine.amp_ramp = (ramp_permille == 0U) ? 1U : ramp_permille;
}

uint32_t MotorCtrl_SineGetCurrentFreqMilliHz(void)
{
    return my_motor.sine.current_freq;
}

uint32_t MotorCtrl_SineGetPhaseAccQ16(void)
{
    return my_motor.sine.phase_acc;
}

uint32_t MotorCtrl_SineGetPhaseStepQ16(void)
{
    return my_motor.sine.phase_step;
}

uint16_t MotorCtrl_SineGetCurrentAmplitudePermille(void)
{
    return my_motor.sine.current_amp;
}
