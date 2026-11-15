/* USER CODE BEGIN Header */

/* USER CODE END Header */

#include "main.h"
#include "usb_device.h"

/* USER CODE BEGIN Includes */
#include "foc.h"
#include "Vofa.h"
#include "encoder.h"
#include "ADCcurrent.h"
#include "cascade.h"
#include "phase_map_diag.h"
#include "angle_lock_diag.h"
#include "speed_diag_channels.h"
#include <math.h>

/* USER CODE END Includes */

/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* USER CODE BEGIN PD */

#define ENC_TEST_MODE        0

/* 旧版：#define PHASE_MAP_TEST_MODE 1，PWM与电流采样相序已经确认正确。 */
#define PHASE_MAP_TEST_MODE  0

/* 旧版：#define ANGLE_LOCK_TEST_MODE 1，四点锁定已确认极对数和方向正确。 */
#define ANGLE_LOCK_TEST_MODE 0

#define USE_CASCADE_LOOP     1

/* 旧版：#define USE_SPEED_CURRENT_LOOP 0，用于0.40A纯电流1秒测试。 */
#define USE_SPEED_CURRENT_LOOP 0

/* 旧版：#define USE_CURRENT_LOOP 1，用于0.40A纯电流1秒测试。 */
#define USE_CURRENT_LOOP     0
#define USE_SPEED_LOOP       0
#define USE_POSITION_LOOP    0

#define TARGET_ANGLE_DEG     300.0f
#define DEG_TO_RAD           0.0174533f

#define TARGET_ID             0.0f

/* 旧版：#define TARGET_IQ 0.35f */
#define TARGET_IQ             0.44f
#define CURRENT_TEST_TIMEOUT_TICKS 20000U

/* 旧版：#define TARGET_SPEED 1.0f，低速摩擦和速度估算影响明显。 */
#define TARGET_SPEED          15.0f 
#define TARGET_ANGLE          5.70f

/* 旧版：#define SPEED_RAMP_ENABLE 1，会先经过1rad/s和2rad/s。 */
#define SPEED_RAMP_ENABLE     0

#define SPEED_RAMP_STEP       1.0f
#define SPEED_RAMP_INTERVAL   500U

#define VOFA_PERIOD_MS        5U
#define VOFA_USE_TEXT         0

#define OFFSET_STEP           0.04f
#define OFFSET_INTERVAL       100U
#define CAL_VOLTAGE_START     0.10f
#define CAL_VOLTAGE_STEP      0.05f
#define CAL_VOLTAGE_MAX       1.50f
#define CAL_VOLTAGE_INTERVAL  500U
#define MOVE_THRESHOLD        0.05f

#define CURRENT_TEST_MODE    1

#define CURRENT_ZERO_VOLT    1.65f

#define CURRENT_V_PER_AMP    0.5f

#define POSITION_SWEEP        0

/* USER CODE END PD */

/* USER CODE BEGIN PM */

/* USER CODE END PM */

ADC_HandleTypeDef hadc3;
DMA_HandleTypeDef hdma_adc3;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;

/* USER CODE BEGIN PV */

/* 旧版相序诊断状态保留，仅在重新启用该模式时参与编译。 */
#if PHASE_MAP_TEST_MODE
static uint32_t g_phase_map_start_tick = 0U;
static phase_map_diag_output_t g_phase_map_output;
#endif
/* 旧版角度锁定诊断状态保留，仅在重新启用该模式时参与编译。 */
#if ANGLE_LOCK_TEST_MODE
static uint32_t g_angle_lock_start_tick = 0U;
static angle_lock_diag_output_t g_angle_lock_output;
#endif

/*
 * 旧版：volatile float g_target_speed = 0.0f;
 * 问题：关闭速度阶梯后仍要等500ms才设定目标，对齐后会中断转矩。
 * 新版：关闭阶梯时上电直接使用TARGET_SPEED。
 */
#if SPEED_RAMP_ENABLE == 0
volatile float g_target_speed = TARGET_SPEED;
#else
volatile float g_target_speed = 0.0f;
#endif
volatile float g_target_angle = TARGET_ANGLE;
volatile float g_cal_voltage  = CAL_VOLTAGE_START;
volatile float g_cal_offset   = 0.0f;
/* USER CODE END PV */

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_TIM1_Init(void);
static void MX_SPI1_Init(void);
static void MX_ADC3_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  MX_GPIO_Init();
  MX_DMA_Init();
  MX_TIM1_Init();
  MX_USB_DEVICE_Init();
  MX_SPI1_Init();
  MX_ADC3_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */

  HAL_TIM_Base_Start(&htim1);

  HAL_TIM_Base_Start(&htim2);

  current_calibrate();

  cascade_init();

#if ENC_TEST_MODE != 1
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
#endif

  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 2100);
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 2100);
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, 2100);

  vofa_init();
  encoder_init();

#if PHASE_MAP_TEST_MODE
  /*
   * === 旧版：上电后立即执行FOC对齐和闭环准备 ===
   * 问题：相序诊断时闭环输出会覆盖低压短脉冲。
   * === 新版：诊断模式只初始化FOC状态，不对齐、不输出闭环 ===
   */
  foc_init();
  g_phase_map_start_tick = HAL_GetTick();
#elif ANGLE_LOCK_TEST_MODE
  /*
   * === 旧版：执行FOC对齐后进入速度电流环 ===
   * 问题：不能独立观察每增加90度电角度时的机械角变化。
   * === 新版：只初始化FOC，由主循环输出低压静态电角度 ===
   */
  foc_init();
  g_angle_lock_start_tick = HAL_GetTick();
#elif ENC_TEST_MODE == 0
  foc_init();
  foc_align();
  HAL_Delay(500);
#else

  foc_init();
#if ENC_TEST_MODE == 2

  foc_align();
  HAL_Delay(500);
#endif
#endif

#if (ENC_TEST_MODE != 1) && !PHASE_MAP_TEST_MODE && !ANGLE_LOCK_TEST_MODE
  HAL_NVIC_SetPriority(TIM1_UP_TIM10_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(TIM1_UP_TIM10_IRQn);
  __HAL_TIM_ENABLE_IT(&htim1, TIM_IT_UPDATE);
#endif

  /* USER CODE END 2 */

  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

#if PHASE_MAP_TEST_MODE
    g_phase_map_output = phase_map_diag_get_output(HAL_GetTick()
                                                 - g_phase_map_start_tick);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1,
                          (uint32_t)(g_phase_map_output.duty_a * PWM_ARR));
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2,
                          (uint32_t)(g_phase_map_output.duty_b * PWM_ARR));
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3,
                          (uint32_t)(g_phase_map_output.duty_c * PWM_ARR));
#elif ANGLE_LOCK_TEST_MODE
    g_angle_lock_output = angle_lock_diag_get_output(HAL_GetTick()
                                                    - g_angle_lock_start_tick);
    if (g_angle_lock_output.active)
    {
      foc_static(g_angle_lock_output.target_angle,
                 g_angle_lock_output.voltage_q);
    }
    else
    {
      foc_stop();
    }
#endif

#if ENC_TEST_MODE == 0
    static uint32_t tick_ramp = 0;
    if (HAL_GetTick() - tick_ramp >= SPEED_RAMP_INTERVAL)
    {
      tick_ramp = HAL_GetTick();
      if (TARGET_SPEED >= 0.0f)
      {
      #if SPEED_RAMP_ENABLE == 0
  g_target_speed = TARGET_SPEED;
#else
  if (g_target_speed < TARGET_SPEED) { g_target_speed += SPEED_RAMP_STEP; }
        if (g_target_speed > TARGET_SPEED) { g_target_speed = TARGET_SPEED; }
#endif
      }
      else
      {
        if (g_target_speed > TARGET_SPEED) { g_target_speed -= SPEED_RAMP_STEP; }
        if (g_target_speed < TARGET_SPEED) { g_target_speed = TARGET_SPEED; }
      }
    }
#endif

/*
 * === 误插入版本（已弃用）===
 * 问题：此处位于VOFA数组声明之前，导致vofa_channel未定义。
 */

#if ENC_TEST_MODE == 3

    {
      static uint32_t tick_cal          = 0;
      static float    s_angle_start   = 0.0f;
      static uint8_t  s_first = 1;
      static uint8_t  s_moved        = 0;
      float    angle_now = encoder_read_mech();

      if (s_first)
      {
        s_angle_start   = angle_now;
        s_first = 0;
      }
      else
      {

        float delta = angle_now - s_angle_start;
        if (delta >  ENC_PI) { delta -= ENC_TWO_PI; }
        if (delta < -ENC_PI) { delta += ENC_TWO_PI; }

        if (delta > MOVE_THRESHOLD || delta < -MOVE_THRESHOLD)
        {
          s_moved = 1;
        }
      }

      if (!s_moved)
      {
        if (HAL_GetTick() - tick_cal >= CAL_VOLTAGE_INTERVAL)
        {
          tick_cal = HAL_GetTick();
          if (g_cal_voltage < CAL_VOLTAGE_MAX)
          {
            g_cal_voltage += CAL_VOLTAGE_STEP;
            if (g_cal_voltage > CAL_VOLTAGE_MAX) { g_cal_voltage = CAL_VOLTAGE_MAX; }
          }
        }
      }
    }
#endif

#if ENC_TEST_MODE == 4

    {
      static uint32_t tick_offset = 0;
      if (HAL_GetTick() - tick_offset >= OFFSET_INTERVAL)
      {
        tick_offset = HAL_GetTick();
        g_cal_offset += OFFSET_STEP;
        if (g_cal_offset >= ENC_TWO_PI) { g_cal_offset -= ENC_TWO_PI; }
        foc_set_offset_trim(g_cal_offset);
      }
    }
#endif

#if (ENC_TEST_MODE == 0) && USE_POSITION_LOOP

#if POSITION_SWEEP
    {
      static const float  s_targets[] = { 5.80f, 5.20f, 4.60f, 5.20f };
      static uint8_t      s_index     = 0;
      static uint32_t     tick_step      = 0;

      if (HAL_GetTick() - tick_step >= 2500U)
      {
        tick_step      = HAL_GetTick();
        s_index     = (uint8_t)((s_index + 1) % 4U);
        g_target_angle = s_targets[s_index];
      }
    }
#endif
#endif

    static uint32_t tick_vofa = 0;
    if (HAL_GetTick() - tick_vofa >= VOFA_PERIOD_MS)
    {
      tick_vofa = HAL_GetTick();

      float vofa_channel[7];

#if PHASE_MAP_TEST_MODE
      {
        float current_a;
        float current_b;
        float current_c;

        current_read(&current_a, &current_b, &current_c);

        vofa_channel[0] = (float)g_phase_map_output.step;
        vofa_channel[1] = current_a;
        vofa_channel[2] = current_b;
        vofa_channel[3] = current_c;
        vofa_channel[4] = g_phase_map_output.duty_a;
        vofa_channel[5] = g_phase_map_output.duty_b;
        vofa_channel[6] = g_phase_map_output.duty_c;
      }
#elif ANGLE_LOCK_TEST_MODE
      {
        float angle_mech;
        float measured_elec;

        /*
         * === 旧版：读取g_encoder_dbg.angle_mech（已弃用）===
         * 问题：角度锁定模式关闭了FOC中断，encoder_update()不再刷新该变量，
         *       导致CH2恒为0、CH3恒为偏移角。
         * === 新版：每个VOFA周期直接读取一次编码器 ===
         */
        /* 使用与闭环相同的突跳拒绝路径，再读取已接受的机械角。 */
        encoder_update();
        angle_mech = g_encoder_dbg.angle_mech;
        measured_elec = angle_lock_diag_measured_elec(angle_mech,
                                                       (float)FOC_POLE_PAIRS,
                                                       ENC_ELEC_DIR,
                                                       ENC_ELEC_OFFSET);

        vofa_channel[0] = (float)g_angle_lock_output.step;
        vofa_channel[1] = g_angle_lock_output.target_angle * 57.2957795f;
        vofa_channel[2] = angle_mech * 57.2957795f;
        vofa_channel[3] = measured_elec * 57.2957795f;
        /* 旧版读取调试缓存；断流后缓存仍保留最后一次占空比。 */
        vofa_channel[4] = (float)g_encoder_dbg.jump_reject_count;
        vofa_channel[5] = (float)__HAL_TIM_GET_COMPARE(&htim1, TIM_CHANNEL_2)
                        / PWM_ARR;
        vofa_channel[6] = (float)__HAL_TIM_GET_COMPARE(&htim1, TIM_CHANNEL_3)
                        / PWM_ARR;
      }
#elif ENC_TEST_MODE == 3

      {
        uint16_t raw_data    = encoder_read_raw();
        uint16_t angle_count  = (uint16_t)(raw_data >> ENC_ANGLE_SHIFT);
        float    angle_mech     = (float)angle_count * ENC_TWO_PI / ENC_COUNTS_PER_REV;
        float    angle     = angle_mech * (float)ENC_POLE_PAIRS * ENC_ELEC_DIR;
        float    offset_now;

        while (angle >= ENC_TWO_PI) { angle -= ENC_TWO_PI; }

        offset_now = FOC_ALIGN_THETA + ENC_HALF_PI - angle;
        while (offset_now <  0.0f)       { offset_now += ENC_TWO_PI; }
        while (offset_now >= ENC_TWO_PI) { offset_now -= ENC_TWO_PI; }

        vofa_channel[0] = g_cal_voltage;
        vofa_channel[1] = angle_mech;
        vofa_channel[2] = offset_now;
        vofa_channel[3] = (float)angle_count;
        vofa_channel[4] = 0.0f;
        vofa_channel[5] = 0.0f;
        vofa_channel[6] = 0.0f;
      }
#elif ENC_TEST_MODE == 4

      vofa_channel[0] = g_cal_offset;
      vofa_channel[1] = g_foc_dbg.angle_mech;
      vofa_channel[2] = g_foc_dbg.error_pos;
      vofa_channel[3] = g_foc_dbg.voltage_q;
      vofa_channel[4] = g_foc_dbg.duty_a;
      vofa_channel[5] = g_foc_dbg.duty_b;
      vofa_channel[6] = g_foc_dbg.duty_c;
#elif ENC_TEST_MODE != 0

      {
        uint16_t raw_data    = encoder_read_raw();
        uint16_t angle_count  = (uint16_t)(raw_data >> ENC_ANGLE_SHIFT);

        float    angle_mech = (float)angle_count * ENC_TWO_PI
                            / ENC_COUNTS_PER_REV * ENC_DIR;
        float    angle;
        float    offset_hint;

        while (angle_mech >= ENC_TWO_PI) { angle_mech -= ENC_TWO_PI; }
        while (angle_mech <  0.0f)       { angle_mech += ENC_TWO_PI; }

        angle = angle_mech * (float)ENC_POLE_PAIRS * ENC_ELEC_DIR;
        while (angle >= ENC_TWO_PI) { angle -= ENC_TWO_PI; }
        while (angle <  0.0f)       { angle += ENC_TWO_PI; }

        offset_hint = FOC_ALIGN_THETA + ENC_HALF_PI - angle;
        while (offset_hint <  0.0f)        { offset_hint += ENC_TWO_PI; }
        while (offset_hint >= ENC_TWO_PI)  { offset_hint -= ENC_TWO_PI; }

        vofa_channel[0] = angle_mech;
        vofa_channel[1] = angle;
        vofa_channel[2] = (float)raw_data;
        vofa_channel[3] = (float)angle_count;
        vofa_channel[4] = offset_hint;
        vofa_channel[5] = 0.0f;
        vofa_channel[6] = 0.0f;
      }
#else
#if USE_SPEED_LOOP

      vofa_channel[0] = g_foc_dbg.target;
      vofa_channel[1] = g_encoder_dbg.speed;
      vofa_channel[2] = g_foc_dbg.error_pos;
      vofa_channel[3] = g_foc_dbg.voltage_q;
      vofa_channel[4] = g_foc_dbg.duty_a;
      vofa_channel[5] = g_foc_dbg.duty_b;
      vofa_channel[6] = g_foc_dbg.duty_c;
#elif USE_POSITION_LOOP

      vofa_channel[0] = g_foc_dbg.target;
      vofa_channel[1] = g_foc_dbg.angle_mech;
      vofa_channel[2] = g_foc_dbg.error_pos;
      vofa_channel[3] = g_foc_dbg.voltage_q;
      vofa_channel[4] = g_foc_dbg.duty_a;
      vofa_channel[5] = g_foc_dbg.duty_b;
      vofa_channel[6] = g_foc_dbg.duty_c;
#else

      {
        static float    s_last_angle  = 0.0f;
        static uint32_t s_last_tick = 0;
        static float    s_speed = 0.0f;
        float    angle_now  = encoder_read_mech();
        uint32_t tick_now  = HAL_GetTick();
        float    speed;

        if (s_last_tick != 0 && tick_now != s_last_tick)
        {
          float delta_angle = angle_now - s_last_angle;
          float dt    = (float)(tick_now - s_last_tick) * 0.001f;
          float speed_instant;

          if (delta_angle >  ENC_PI) { delta_angle -= ENC_TWO_PI; }
          if (delta_angle < -ENC_PI) { delta_angle += ENC_TWO_PI; }

          speed_instant = delta_angle / dt;

          if (speed_instant > 15.0f || speed_instant < -15.0f)
          {
            speed_instant = s_speed;
          }

          s_speed += (speed_instant - s_speed) * (dt / 0.5f);
        }

        s_last_angle  = angle_now;
        s_last_tick = tick_now;
        speed        = s_speed;

        vofa_channel[0] = g_target_speed;
        vofa_channel[1] = speed;

        {

          float offset_now = g_foc_dbg.angle + ENC_HALF_PI
                           - (float)ENC_POLE_PAIRS * angle_now * ENC_ELEC_DIR;
          while (offset_now <  0.0f)       { offset_now += ENC_TWO_PI; }
          while (offset_now >= ENC_TWO_PI) { offset_now -= ENC_TWO_PI; }
          vofa_channel[2] = offset_now;
        }
        vofa_channel[3] = g_foc_dbg.voltage_q;
        vofa_channel[4] = g_foc_dbg.duty_a;
        vofa_channel[5] = g_foc_dbg.duty_b;
        vofa_channel[6] = g_foc_dbg.duty_c;
      }
#endif
#endif

/* 旧版：#if CURRENT_TEST_MODE && !PHASE_MAP_TEST_MODE，会覆盖角度锁定诊断通道。 */
#if CURRENT_TEST_MODE && !PHASE_MAP_TEST_MODE && !ANGLE_LOCK_TEST_MODE
    {
      static uint32_t tick_current = 0;
      static float    current_a    = 0.0f;
      static float    current_b    = 0.0f;
      static float    current_c    = 0.0f;

      if (HAL_GetTick() - tick_current >= 5)
      {
        uint32_t tick_wait = HAL_GetTick();

        tick_current = HAL_GetTick();

        while (__HAL_TIM_GET_COUNTER(&htim1) < 3800)
        {
          if (HAL_GetTick() - tick_wait > 2)
          {
            break;
          }
        }

        current_read(&current_a, &current_b, &current_c);
      }

      {
        float elec = g_encoder_dbg.angle_mech * (float)FOC_POLE_PAIRS * ENC_ELEC_DIR
                   + ENC_ELEC_OFFSET;
        float c, sn, i_alpha, i_beta, id_now, iq_now;

        while (elec >= 6.28318530f) { elec -= 6.28318530f; }
        while (elec <  0.0f)        { elec += 6.28318530f; }

        c  = cosf(elec);
        sn = sinf(elec);

        i_alpha = current_a;
        i_beta  = (current_a + 2.0f * current_b) * 0.57735f;

        id_now =  i_alpha * c + i_beta * sn;
        iq_now = -i_alpha * sn + i_beta * c;

        vofa_channel[4] = id_now;
        vofa_channel[5] = iq_now;
      }

      vofa_channel[0] = g_foc_dbg.current_q_ref;
      vofa_channel[1] = g_foc_dbg.current_q;
      vofa_channel[2] = g_foc_dbg.current_d;
      vofa_channel[3] = g_foc_dbg.voltage_q;
      vofa_channel[4] = g_encoder_dbg.angle_mech * 57.2958f;
      vofa_channel[6] = g_encoder_dbg.speed;

#if USE_SPEED_CURRENT_LOOP

      /*
       * === 旧版：三相电流与电角度诊断通道（已暂停使用）===
       * 问题：恢复速度环后无法同时观察目标速度、实测速度和异常帧计数。
       * === 新版：速度环与编码器诊断通道 ===
       */
      speed_diag_map_channels(vofa_channel,
                              g_target_speed,
                              g_encoder_dbg.speed,
                              cascade_get_iq_cmd(),
                              g_foc_dbg.current_q,
                              g_foc_dbg.current_d,
                              g_encoder_dbg.jump_reject_count,
                              g_foc_dbg.voltage_q);
#if USE_CASCADE_LOOP
      vofa_channel[0] = TARGET_ANGLE_DEG;
      vofa_channel[1] = g_encoder_dbg.angle_mech * 57.29578f;
      vofa_channel[2] = cascade_get_error_pos();
      vofa_channel[3] = cascade_get_v_cmd();
      vofa_channel[4] = cascade_get_iq_cmd();
      vofa_channel[5] = g_encoder_dbg.speed;
      vofa_channel[6] = g_foc_dbg.current_q;
#endif
#endif
    }
#endif

#if VOFA_USE_TEXT

      (void)vofa_send_text(vofa_channel, 7);
#else

      vofa_begin();
      vofa_put(vofa_channel[0]);
      vofa_put(vofa_channel[1]);
      vofa_put(vofa_channel[2]);
      vofa_put(vofa_channel[3]);
      vofa_put(vofa_channel[4]);
      vofa_put(vofa_channel[5]);
      vofa_put(vofa_channel[6]);
      (void)vofa_end();
#endif
    }
  }
  /* USER CODE END 3 */
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_ADC3_Init(void)
{

  ADC_ChannelConfTypeDef sConfig = {0};

  hadc3.Instance = ADC3;
  hadc3.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc3.Init.Resolution = ADC_RESOLUTION_12B;
  hadc3.Init.ScanConvMode = ENABLE;
  hadc3.Init.ContinuousConvMode = DISABLE;
  hadc3.Init.DiscontinuousConvMode = DISABLE;
  hadc3.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
  hadc3.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T2_TRGO;
  hadc3.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc3.Init.NbrOfConversion = 2;
  hadc3.Init.DMAContinuousRequests = ENABLE;
  hadc3.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc3) != HAL_OK)
  {
    Error_Handler();
  }

  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_15CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc3, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = 2;
  if (HAL_ADC_ConfigChannel(&hadc3, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

}

static void MX_SPI1_Init(void)
{

  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }

}

static void MX_TIM1_Init(void)
{

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_CENTERALIGNED1;
  htim1.Init.Period = 4199;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 1;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_OC4REF;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 2100;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.Pulse = 0;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM2;
  sConfigOC.Pulse = 3800;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_ENABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_ENABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_ENABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_TIM_MspPostInit(&htim1);

}

static void MX_TIM2_Init(void)
{

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_SlaveConfigTypeDef sSlaveConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 3799;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_OnePulse_Init(&htim2, TIM_OPMODE_SINGLE) != HAL_OK)
  {
    Error_Handler();
  }
  sSlaveConfig.SlaveMode = TIM_SLAVEMODE_TRIGGER;
  sSlaveConfig.InputTrigger = TIM_TS_ITR0;
  if (HAL_TIM_SlaveConfigSynchro(&htim2, &sSlaveConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }

}

static void MX_DMA_Init(void)
{

  __HAL_RCC_DMA2_CLK_ENABLE();

  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);

}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();

  HAL_GPIO_WritePin(cs_MT6701_GPIO_Port, cs_MT6701_Pin, GPIO_PIN_SET);

  GPIO_InitStruct.Pin = cs_MT6701_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(cs_MT6701_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

void TIM1_UP_TIM10_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&htim1);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM1)
  {

    __HAL_TIM_ENABLE(&htim2);

#if ENC_TEST_MODE == 3
    foc_static(FOC_ALIGN_THETA, g_cal_voltage);
#elif ENC_TEST_MODE == 4
    foc_close_position(g_target_angle);
#elif ENC_TEST_MODE == 2

    ;
#elif USE_CASCADE_LOOP
    cascade_position(TARGET_ANGLE_DEG * DEG_TO_RAD);
#elif USE_SPEED_CURRENT_LOOP
    cascade_speed(g_target_speed);
#elif USE_CURRENT_LOOP
/*
 * === 旧版：固定电流持续输出（已弃用）===
 * 问题：电机停转时会长时间堵转发热。
 * === 新版：0.40A只输出1秒，之后自动归零 ===
 */
    foc_close_current(TARGET_IQ, TARGET_ID);
#elif USE_SPEED_LOOP
    foc_close_speed(g_target_speed);
#elif USE_POSITION_LOOP
    foc_close_position(g_target_angle);
#else
    foc_open(g_target_speed);
#endif
  }
}

/* USER CODE END 4 */

void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */

  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT

void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */

  /* USER CODE END 6 */
}
#endif
