/* ============================================================
 *  foc.c  ——  FOC 开环控制（直白版）
 * ------------------------------------------------------------
 *  整条链路（从上往下读）：
 *
 *    期望转速 w_ref
 *      → 电角速度 we = w_ref × 极对数
 *      → 电角度 theta = 累加 we × Ts
 *      → 电压 Uq = 斜率 × |we| + 低速补偿
 *      → 反 Park：  Ualpha = -Uq·sin(theta)
 *                   Ubeta  =  Uq·cos(theta)
 *      → 反 Clarke：Ua =  Ualpha
 *                   Ub = -0.5·Ualpha + 0.866·Ubeta
 *                   Uc = -0.5·Ualpha - 0.866·Ubeta
 *      → 加共模：   Ucom = -(最大 + 最小) / 2
 *      → 转占空比：T = 0.5 + U / 电源电压
 *      → 写比较值：CCR = T × 4199
 * ============================================================ */

#include "foc.h"
#include "encoder.h"
#include <math.h>

/* CubeMX 在 main.c 里定义的定时器，这里引用 */
extern TIM_HandleTypeDef htim1;

/* ---------- 内部变量（只在本文件用） ---------- */
static float g_theta_e = 0.0f;    /* 当前开环累加的电角度，弧度 */
static float g_we      = 0.0f;    /* 当前电角速度，弧度/秒 */
static float g_mech_measured = 0.0f;  /* 编码器实测机械角，给调试看 */
static float g_pos_error     = 0.0f;  /* 最近一次位置误差，给调试看 */
static float g_pos_integral  = 0.0f;  /* 位置环积分项，单位伏 */
static float g_elec_offset_trim = 0.0f; /* 电角度微调，offset 扫描标定用 */
static float g_speed_integral   = 0.0f; /* 速度环积分项，单位伏 */
static uint32_t g_stall_count   = 0;    /* 速度环"卡住"计数 */

/* 主循环改这个值，中断里会立刻用上 */
void foc_set_elec_offset_trim(float trim_rad)
{
    g_elec_offset_trim = trim_rad;
}

/* ---------- 调试观测量：给 VOFA+ 看波形 ----------
   中断里写、主循环里读。里面每个成员都是 32 位 float，
   单个成员的读写不会被拆开，所以不需要加锁。 */
volatile foc_debug_t g_foc_dbg;

/* 一个圆周是 2π */
#define TWO_PI   6.28318530f

/* 半个圆周是 π（不叫 PI，避免和 math.h / arm_math.h 的宏撞名） */
#define FOC_PI   3.14159265f

/* √3/2 = 0.866，反 Clarke 要用 */
#define SQRT3_2  0.86602540f


/* ============================================================
 *  初始化
 * ============================================================ */
void foc_init(void)
{
    g_theta_e        = 0.0f;
    g_we             = 0.0f;
    g_pos_integral   = 0.0f;
    g_speed_integral = 0.0f;
}


/* ============================================================
 *  速度生成器：电角速度 → 电压 Uq
 * ------------------------------------------------------------
 *  开环没有编码器，测不到真实转速，所以用最简单的线性关系：
 *
 *      Uq = FOC_KVF × |we| + FOC_U_OFFSET
 *
 *  转速越高给越大的电压，保证磁场够强不会失步。
 *  we 取绝对值，因为转向由角度 theta 的方向决定。
 * ============================================================ */
float foc_speed_generator(float w_e)
{
    float uq;

    if (w_e < 0.0f)
    {
        w_e = -w_e;                 /* 取绝对值 */
    }

    uq = FOC_KVF * w_e + FOC_U_OFFSET;

    if (uq > VQ_MAX)                /* 不能超过 6.92V，否则波形削顶 */
    {
        uq = VQ_MAX;
    }

    return uq;
}


/* ============================================================
 *  转子预定位
 * ------------------------------------------------------------
 *  电角度固定不动，给一个小的 q 轴电压。
 *  电压矢量停在空间某个固定方向，转子磁钢会被吸过去对齐。
 * ============================================================ */
void foc_align(void)
{
    float Uq;
    float Ualpha, Ubeta;
    float Ua, Ub, Uc;
    float Umax, Umin, Ucom;
    float Ta, Tb, Tc;

    Uq = FOC_ALIGN_VOLTAGE;         /* 专用预定位电压，必须够大才吸得动 */

    /* 让开环中断接下来继续用这个角度（w_cmd=0 时 theta 就停在这里） */
    g_theta_e = FOC_ALIGN_THETA;

    /* 反 Park：把电压矢量加在 FOC_ALIGN_THETA + 90° 的方向上
       （theta=0 时就是 Ualpha=0、Ubeta=Uq，和以前一样） */
    Ualpha = -Uq * sinf(FOC_ALIGN_THETA);
    Ubeta  =  Uq * cosf(FOC_ALIGN_THETA);

    /* 反 Clarke */
    Ua =  Ualpha;
    Ub = -0.5f * Ualpha + SQRT3_2 * Ubeta;
    Uc = -0.5f * Ualpha - SQRT3_2 * Ubeta;

    /* 共模注入 */
    Umax = Ua;
    if (Ub > Umax) { Umax = Ub; }
    if (Uc > Umax) { Umax = Uc; }
    Umin = Ua;
    if (Ub < Umin) { Umin = Ub; }
    if (Uc < Umin) { Umin = Uc; }
    Ucom = -(Umax + Umin) * 0.5f;

    /* 转占空比 */
    Ta = 0.5f + (Ua + Ucom) / VDC_BUS;
    Tb = 0.5f + (Ub + Ucom) / VDC_BUS;
    Tc = 0.5f + (Uc + Ucom) / VDC_BUS;

    /* 写死三路比较值 */
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, (uint32_t)(Ta * PWM_ARR));
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, (uint32_t)(Tb * PWM_ARR));
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, (uint32_t)(Tc * PWM_ARR));
}


/* ============================================================
 *  停机
 * ============================================================ */
void foc_stop(void)
{
    uint32_t half = (uint32_t)(0.5f * PWM_ARR);

    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, half);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, half);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, half);
}


/* ============================================================
 *  小工具：角度归一化
 * ============================================================ */

/* 折回 0 ~ 2π */
static float wrap_two_pi(float angle)
{
    while (angle >= TWO_PI) { angle -= TWO_PI; }
    while (angle <  0.0f)   { angle += TWO_PI; }
    return angle;
}

/* 折回 -π ~ +π，用来算"走近路"的角度误差 */
static float wrap_pi(float angle)
{
    while (angle >  FOC_PI) { angle -= TWO_PI; }
    while (angle < -FOC_PI) { angle += TWO_PI; }
    return angle;
}


/* ============================================================
 *  输出级：电角度 + q 轴电压  →  三相占空比  →  CCR
 * ------------------------------------------------------------
 *  这一段的输入只有两个量：
 *      theta : 电角度（开环是自己算的，闭环是编码器给的）
 *      Uq    : q 轴电压（开环是 V/f 曲线给的，闭环是位置环给的）
 *  开环和闭环共用它，避免两份重复代码
 * ============================================================ */
static void foc_output(float theta, float Uq)
{
    float Ualpha, Ubeta;            /* 静止两相 */
    float Ua, Ub, Uc;               /* 三相电压 */
    float Umax, Umin, Ucom;         /* 共模注入用 */
    float Ta, Tb, Tc;               /* 三相占空比 */
    uint32_t ccr1, ccr2, ccr3;      /* 写进定时器的值 */

    /* ---------- 反 Park 变换 ----------
       dq 坐标转 αβ 坐标（Ud = 0，所以只剩下两项） */
    Ualpha = -Uq * sinf(theta);
    Ubeta  =  Uq * cosf(theta);

    /* ---------- 反 Clarke 变换 ----------
       αβ 坐标转三相 abc */
    Ua =  Ualpha;
    Ub = -0.5f * Ualpha + SQRT3_2 * Ubeta;
    Uc = -0.5f * Ualpha - SQRT3_2 * Ubeta;

    /* ---------- 共模注入（SVPWM） ----------
       把三相整体上下平移，不改变线电压，但能提高电压利用率 */
    Umax = Ua;
    if (Ub > Umax) { Umax = Ub; }
    if (Uc > Umax) { Umax = Uc; }

    Umin = Ua;
    if (Ub < Umin) { Umin = Ub; }
    if (Uc < Umin) { Umin = Uc; }

    Ucom = -(Umax + Umin) * 0.5f;

    Ua = Ua + Ucom;
    Ub = Ub + Ucom;
    Uc = Uc + Ucom;

    /* ---------- 电压转占空比 ----------
       电压 0V → 占空比 50%
       电压 +6V → 占空比 100%
       电压 -6V → 占空比 0% */
    Ta = 0.5f + Ua / VDC_BUS;
    Tb = 0.5f + Ub / VDC_BUS;
    Tc = 0.5f + Uc / VDC_BUS;

    /* 限制在 0 ~ 1 之间，防止越界 */
    if (Ta < 0.0f) { Ta = 0.0f; }
    if (Ta > 1.0f) { Ta = 1.0f; }
    if (Tb < 0.0f) { Tb = 0.0f; }
    if (Tb > 1.0f) { Tb = 1.0f; }
    if (Tc < 0.0f) { Tc = 0.0f; }
    if (Tc > 1.0f) { Tc = 1.0f; }

    /* ---------- 存观测量，给 VOFA+ 用 ---------- */
    g_foc_dbg.theta  = theta;
    g_foc_dbg.uq     = Uq;
    g_foc_dbg.duty_a = Ta;
    g_foc_dbg.duty_b = Tb;
    g_foc_dbg.duty_c = Tc;

    /* ---------- 写进定时器：占空比 × 4199 = 比较值 ---------- */
    ccr1 = (uint32_t)(Ta * PWM_ARR);
    ccr2 = (uint32_t)(Tb * PWM_ARR);
    ccr3 = (uint32_t)(Tc * PWM_ARR);

    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, ccr1);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, ccr2);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, ccr3);
}


/* ============================================================
 *  开环控制  foc_step_openloop
 * ------------------------------------------------------------
 *  输入：期望机械角速度 w_ref，单位 弧度/秒
 *  角度自己积分出来，电压按 V/f 曲线给
 * ============================================================ */
void foc_step_openloop(float w_ref_mech)
{
    float we;                       /* 电角速度 */
    float theta;                    /* 电角度 */
    float Uq;                       /* q 轴电压 */

    /* ---------- 第 1 步：机械角速度 → 电角速度 ---------- */
    we = w_ref_mech * (float)FOC_POLE_PAIRS;
    g_we = we;

    /* ---------- 第 2 步：电角度累加 ----------
       开环的关键：角度不是测量出来的，是我们自己算的。 */
    theta = g_theta_e + we * FOC_TS;
    theta = wrap_two_pi(theta);
    g_theta_e = theta;

    /* ---------- 第 3 步：算电压 Uq（V/f 曲线） ---------- */
    Uq = foc_speed_generator(we);

    /* ---------- 记录 --- */
    g_foc_dbg.w_ref = w_ref_mech;
    g_foc_dbg.we    = we;

    /* ---------- 第 4~9 步：交给输出级 --- */
    foc_output(theta, Uq);
}


/* ============================================================
 *  静态输出（标定用）
 * ------------------------------------------------------------
 *  给一个固定方向、固定大小的电压矢量，不做任何闭环。
 *  转子会被磁场吸到某个固定位置，用来反推电角度零位。
 * ============================================================ */
void foc_step_static(float theta_rad, float uq)
{
    g_foc_dbg.w_ref = theta_rad;
    g_foc_dbg.we    = 0.0f;
    foc_output(wrap_two_pi(theta_rad), uq);
}


/* ============================================================
 *  位置闭环  foc_step_position  ★ L1 ★
 * ------------------------------------------------------------
 *  输入：目标机械角度 target_mech_rad，单位 弧度，0 ~ 2π
 *
 *  和开环的区别只有两点：
 *    1. theta 不再自己积分，而是编码器实测（乘极对数 + 零位偏移）
 *    2. Uq 不再是 V/f 曲线，而是"位置误差 × Kp"
 *
 *  注意：这里 Uq 是**电压**不是力矩。要真正控力矩得上电流环（L3）。
 * ============================================================ */
void foc_step_position(float target_mech_rad)
{
    float mech_rad;                 /* 编码器实测机械角 */
    float elec_rad;                 /* 换算出的电角度（= 转子 d 轴） */
    float error_rad;                /* 位置误差，-π ~ +π */
    float Uq;                       /* q 轴电压 */

    /* ---------- 第 1 步：读编码器，得到机械角和转速 ---------- */
    encoder_update();
    mech_rad = g_encoder_dbg.mech_rad;
    g_mech_measured = mech_rad;

    /* ---------- 第 2 步：机械角 → 电角度 ----------
       乘极对数，再加上标定出来的零位偏移，才真正对得上转子 d 轴 */
    elec_rad = wrap_two_pi(mech_rad * (float)FOC_POLE_PAIRS
                         + ENC_ELEC_OFFSET
                         + g_elec_offset_trim);

    /* ---------- 第 3 步：算位置误差，走近路 ----------
       目标 6.2、当前 0.1 时，直接相减是 6.1，走近路应该是 -0.18 */
    error_rad = wrap_pi(target_mech_rad - mech_rad);
    g_pos_error = error_rad;

    /* ---------- 第 4 步：PID 控制 → 电压 ----------
       P 项：误差越大，拉回来的力气越大
       I 项：把残差持续累积，克服静摩擦造成的稳态误差
       D 项：转得越快，反向刹车越多，防止冲过头来回摆 */
    g_pos_integral += FOC_POS_KI * error_rad * FOC_TS;

    /* 抗积分饱和：积分项自己也不能超过电压限幅，否则退饱和会很慢 */
    if (g_pos_integral >  FOC_UQ_LIMIT) { g_pos_integral =  FOC_UQ_LIMIT; }
    if (g_pos_integral < -FOC_UQ_LIMIT) { g_pos_integral = -FOC_UQ_LIMIT; }

    Uq = FOC_POS_KP * error_rad
       + g_pos_integral
       - FOC_POS_KD * g_encoder_dbg.speed_radps;

    /* 转矩方向：矢量和 d 轴相对方向反了时用 -1 翻转 */
    Uq = FOC_TORQUE_SIGN * Uq;

    /* 硬限幅：再大就过流了 */
    if (Uq >  FOC_UQ_LIMIT) { Uq =  FOC_UQ_LIMIT; }
    if (Uq < -FOC_UQ_LIMIT) { Uq = -FOC_UQ_LIMIT; }

    /* ---------- 记录 --- */
    g_foc_dbg.w_ref         = target_mech_rad;
    g_foc_dbg.we            = g_encoder_dbg.speed_radps * (float)FOC_POLE_PAIRS;
    g_foc_dbg.mech_measured = mech_rad;
    g_foc_dbg.pos_error     = error_rad;

    /* ---------- 第 5~10 步：交给输出级 --- */
    foc_output(elec_rad, Uq);
}


/* ============================================================
 *  速度闭环  foc_step_speed  ★ L2 ★
 * ------------------------------------------------------------
 *  输入：目标机械角速度 target_speed_radps，单位 弧度/秒
 *        正数正转，负数反转，0 停住
 *
 *  和位置环的区别：反馈量从"位置"换成"速度"，输出电压由
 *      V/f 前馈（按目标转速给一个基准电压）
 *    + PI 修正（把实测速度和目标之间的差补掉）
 *  组成。加前馈的好处是 PI 不用从零开始堆积分，响应快很多。
 * ============================================================ */
void foc_step_speed(float target_speed_radps)
{
    float mech_rad;                 /* 编码器实测机械角 */
    float speed_radps;              /* 编码器实测机械角速度 */
    float elec_rad;                 /* 换算出的电角度 */
    float error_radps;              /* 速度误差 */
    float feedforward;              /* V/f 前馈电压 */
    float we;                       /* 目标电角速度 */
    float Uq;
    float direction;

    /* ---------- 第 1 步：读编码器，同时拿到位置和速度 ---------- */
    encoder_update();
    mech_rad    = g_encoder_dbg.mech_rad;
    speed_radps = g_encoder_dbg.speed_radps;

    /* ---------- 第 2 步：机械角 → 电角度 ---------- */
    elec_rad = wrap_two_pi(mech_rad * (float)FOC_POLE_PAIRS
                         + ENC_ELEC_OFFSET
                         + g_elec_offset_trim);

    /* ---------- 第 3 步：速度误差 ---------- */
    error_radps = target_speed_radps - speed_radps;

    /* ---------- 第 4 步：V/f 前馈 ----------
       按"目标转速"算一个基准电压，PI 只负责修正剩下的差。
       方向和目标转速一致；幅值只看转速绝对值。 */
    we = target_speed_radps * (float)FOC_POLE_PAIRS;
    direction = (we >= 0.0f) ? 1.0f : -1.0f;
    if (we < 0.0f) { we = -we; }

    feedforward = direction * (FOC_KVF * we + FOC_U_OFFSET);

    /* ---------- 第 5 步：PI 修正 ---------- */
    g_speed_integral += FOC_SPD_KI * error_radps * FOC_TS;

    /* 抗积分饱和 */
    if (g_speed_integral >  FOC_UQ_LIMIT) { g_speed_integral =  FOC_UQ_LIMIT; }
    if (g_speed_integral < -FOC_UQ_LIMIT) { g_speed_integral = -FOC_UQ_LIMIT; }

    Uq = feedforward
       + FOC_SPD_KP * error_radps
       + g_speed_integral;

    /* ---------- 起动冲击（对付静摩擦） ----------
       这台电机静摩擦明显大于动摩擦，从静止起步会卡住。
       检测"目标非零、实测几乎为 0"，就周期性补一个额外电压把它踹起来。 */
    if (target_speed_radps > 0.2f || target_speed_radps < -0.2f)
    {
        if (speed_radps < 0.1f && speed_radps > -0.1f)
        {
            g_stall_count++;
            if (g_stall_count > FOC_STALL_TIME)
            {
                Uq += direction * FOC_START_BOOST;
            }
            /* 踹 0.2 秒后清零，停一下再踹，避免连续大电流 */
            if (g_stall_count > FOC_BOOST_CYCLE)
            {
                g_stall_count = 0;
            }
        }
        else
        {
            g_stall_count = 0;      /* 转起来了，取消冲击 */
        }
    }
    else
    {
        g_stall_count = 0;          /* 目标为 0，不需要 */
    }

    /* 转矩方向 */
    Uq = FOC_TORQUE_SIGN * Uq;

    /* 硬限幅 */
    if (Uq >  FOC_UQ_LIMIT) { Uq =  FOC_UQ_LIMIT; }
    if (Uq < -FOC_UQ_LIMIT) { Uq = -FOC_UQ_LIMIT; }

    /* ---------- 记录 --- */
    g_foc_dbg.w_ref         = target_speed_radps;
    g_foc_dbg.we            = speed_radps * (float)FOC_POLE_PAIRS;
    g_foc_dbg.mech_measured = mech_rad;
    g_foc_dbg.pos_error     = error_radps;   /* 这里放的是速度误差 */

    /* ---------- 输出 --- */
    foc_output(elec_rad, Uq);
}
