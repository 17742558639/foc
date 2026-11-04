/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "foc.h"
#include "Vofa.h"
#include "encoder.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* ==================== 运行模式 ==================== */
/* 0 = 正常运行（开环 / 位置环 / 速度环，由下面两个开关决定）
   1 = 只读编码器（不启动 PWM，电机自由，手转看角度）
   2 = 静态标定（固定电压矢量吸住转子，ch4 给出建议 offset）
   3 = 升压扫描（0.1V 起每 0.5s 加 0.05V，找启动电压后自动冻结）
   4 = offset 扫描（微调量转一圈，看电机何时开始朝目标走） */
#define ENC_TEST_MODE        0

/* ==================== 控制方式（三选一） ==================== */
/* USE_SPEED_LOOP=1     → 速度闭环，按 TARGET_SPEED 转
   USE_POSITION_LOOP=1  → 位置闭环，转到 TARGET_ANGLE 停住
   两个都 0              → 开环 V/f */
#define USE_SPEED_LOOP       0
#define USE_POSITION_LOOP    1

/* ==================== 目标值 ==================== */
#define TARGET_SPEED          1.0f       /* 目标转速，rad/s（负数反转） */
#define TARGET_ANGLE          5.70f     /* 目标机械角，rad（0 ~ 6.283） */

/* ==================== 转速斜坡 ==================== */
#define SPEED_RAMP_STEP       0.5f      /* 每步加多少，rad/s */
#define SPEED_RAMP_INTERVAL   500U      /* 每步多少毫秒 */

/* ==================== VOFA+ 波形 ==================== */
#define VOFA_PERIOD_MS        5U        /* 发送周期，ms（5ms = 200Hz） */
#define VOFA_USE_TEXT         0         /* 1 = FireWater 文本，0 = JustFloat 二进制 */

/* ==================== 标定参数 ==================== */
#define OFFSET_STEP           0.04f     /* offset 扫描步长，rad/100ms */
#define OFFSET_INTERVAL       100U      /* 每步多少毫秒 */
#define CAL_VOLTAGE_START     0.10f     /* 升压扫描起始电压，V */
#define CAL_VOLTAGE_STEP      0.05f     /* 每步加多少，V */
#define CAL_VOLTAGE_MAX       1.50f     /* 最高升到多少，V */
#define CAL_VOLTAGE_INTERVAL  500U      /* 每步多少毫秒 */
#define MOVE_THRESHOLD        0.05f     /* 转过这么多就认为"吸动了"，rad */

/* 位置环验证：1 = 每 2.5 秒自动换目标，0 = 固定保持一点 */
#define POSITION_SWEEP        0

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim1;

/* USER CODE BEGIN PV */
/* 主循环写、中断读，必须 volatile */
volatile float g_target_speed = 0.0f;               /* 目标转速，rad/s */
volatile float g_target_angle = TARGET_ANGLE;       /* 目标机械角，rad */
volatile float g_cal_voltage  = CAL_VOLTAGE_START;  /* 升压扫描当前电压，V */
volatile float g_cal_offset   = 0.0f;               /* offset 扫描微调量，rad */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM1_Init(void);
static void MX_SPI1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM1_Init();
  MX_USB_DEVICE_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */
  /* 只用 3 路单端 PWM！
     驱动板（L6234 / SimpleFOCShield）自己会把每一路补成上下桥，
     死区和防直通都在芯片内部做，所以这里绝对不要再去开 CHxN：
       1) 开了以后 CH1N/CH2N/CH3N 引脚也会输出电平，接错线会和驱动板打架；
       2) 两边的死区会叠加，波形反而更差。
     也就是说 BDTR 里那个 DeadTime=100 现在不参与任何输出。 */
#if ENC_TEST_MODE != 1
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
#endif
  /* 初始占空比统一设 50%：ARR=4199，比较值 2100，
     3 路都是 50% 时三相电压互相抵消，电机不出力 */
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 2100);
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 2100);
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, 2100);

  /* ==================== FOC 开环启动 ==================== */
  vofa_init();        /* VOFA+：USB 已由 MX_USB_DEVICE_Init 起好，这里只清计数 */
  encoder_init();     /* MT6701：片选拉高、清空内部状态 */

#if ENC_TEST_MODE == 0
  foc_init();
  foc_align();        /* 转子预定位：固定电压矢量把转子吸住 */
  HAL_Delay(500);     /* 等转子停稳，此时中断还没开，CCR 保持不变 */
#else
  /* 模式 1 / 2：也要初始化 FOC 内部变量（angle=0、voltage_q=0），但不做长时间预定位 */
  foc_init();
#if ENC_TEST_MODE == 2
  /* 标定模式需要转子被固定矢量吸住，所以这里同样要预定位 */
  foc_align();
  HAL_Delay(500);
#endif
#endif

  /* 打开 TIM1 更新中断，20kHz 触发一次 FOC 计算 */
#if ENC_TEST_MODE != 1
  HAL_NVIC_SetPriority(TIM1_UP_TIM10_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(TIM1_UP_TIM10_IRQn);
  __HAL_TIM_ENABLE_IT(&htim1, TIM_IT_UPDATE);
#endif
  /* ==================================================== */
  
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* 慢斜坡：0 → TARGET_SPEED，每 0.5 秒加 0.5 rad/s。
       给到 g_target_speed 后，TIM1 中断里 foc_open(g_target_speed) 每 50us 读一次。
       只在正常运行模式下爬坡；标定模式必须让 g_target_speed 保持 0，矢量才不动 */
#if ENC_TEST_MODE == 0
    static uint32_t tick_ramp = 0;
    if (HAL_GetTick() - tick_ramp >= SPEED_RAMP_INTERVAL)
    {
      tick_ramp = HAL_GetTick();
      if (TARGET_SPEED >= 0.0f)          /* 正转：往正方向爬 */
      {
        if (g_target_speed < TARGET_SPEED) { g_target_speed += SPEED_RAMP_STEP; }
        if (g_target_speed > TARGET_SPEED) { g_target_speed = TARGET_SPEED; }
      }
      else                                  /* 反转：往负方向爬 */
      {
        if (g_target_speed > TARGET_SPEED) { g_target_speed -= SPEED_RAMP_STEP; }
        if (g_target_speed < TARGET_SPEED) { g_target_speed = TARGET_SPEED; }
      }
    }
#endif

#if ENC_TEST_MODE == 3
    /* 升压扫描：每 0.5 秒把电压抬高 0.05V，从 0.1V 一直到 1.5V。
       电机在哪一档"啪"一下被吸动，那一档就是它的启动电压。
       ★ 安全设计：一旦检测到转子真的动了，立刻冻结电压，不再往上升，
         避免"电阻其实很小 → 升压过头 → 过流烧芯片"。 */
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
        /* 判断"相对起点转过了多少"，而不是逐次累加绝对值——
           逐次累加会被编码器 ±1 计数的噪声喂出一个假的"大幅移动" */
        float delta = angle_now - s_angle_start;
        if (delta >  ENC_PI) { delta -= ENC_TWO_PI; }
        if (delta < -ENC_PI) { delta += ENC_TWO_PI; }

        if (delta > MOVE_THRESHOLD || delta < -MOVE_THRESHOLD)
        {
          s_moved = 1;
        }
      }

      /* 只有"还没动"的时候才继续升压 */
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
    /* offset 扫描：每 100ms 把电角度微调往前推 0.03 rad，
       大约 21 秒转完一圈。位置环一直在跑，
       扫到"正确"的那个角度时，电机会明显开始往目标走。 */
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
    /* 位置环验证：每 2.5 秒换一个目标角，看电机能不能准确跟过去。
       步长只有 0.6 弧度（34°），小步、安全、看得清。
       想固定目标就把上面的 POSITION_SWEEP 改成 0 */
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

    /* ---------- VOFA+ 示波器：每 5ms 推一帧（200Hz） ----------
       7 个通道，顺序就是 VOFA+ 里的通道 0~6：
         0: g_target_speed          给定机械角速度 rad/s
         1: g_foc_dbg.speed_e   电角速度 rad/s
         2: g_foc_dbg.angle 电角度 rad
         3: g_foc_dbg.voltage_q   q 轴电压 V
         4~6: 三相占空比 0~1 */
    static uint32_t tick_vofa = 0;
    if (HAL_GetTick() - tick_vofa >= VOFA_PERIOD_MS)
    {
      tick_vofa = HAL_GetTick();

      float vofa_channel[7];

#if ENC_TEST_MODE == 3
      /* ===== 升压扫描 =====
         ch0: 当前电压 V      ch1: 机械角 rad
         ch2: 建议 offset     ch3: 14 位计数 */
      {
        uint16_t raw_data    = encoder_read_raw();
        uint16_t angle_count  = (uint16_t)(raw_data >> ENC_ANGLE_SHIFT);
        float    angle_mech     = (float)angle_count * ENC_TWO_PI / ENC_COUNTS_PER_REV;
        float    angle     = angle_mech * (float)ENC_POLE_PAIRS;
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
      /* offset 扫描：ch0 当前微调值 / ch1 实测角 / ch2 误差 / ch3 voltage_q */
      vofa_channel[0] = g_cal_offset;
      vofa_channel[1] = g_foc_dbg.angle_mech;
      vofa_channel[2] = g_foc_dbg.error_pos;
      vofa_channel[3] = g_foc_dbg.voltage_q;
      vofa_channel[4] = g_foc_dbg.duty_a;
      vofa_channel[5] = g_foc_dbg.duty_b;
      vofa_channel[6] = g_foc_dbg.duty_c;
#elif ENC_TEST_MODE != 0
      /* ===== 编码器调试（模式 1 / 2）=====
         通道 0: 机械角 rad        1: 电角度 rad（未加 offset）
         通道 2: SPI 原始 16 位    3: 14 位角度计数 0~16383
         通道 4: 建议的 offset（把电角度归零所需的偏移量） */
      {
        uint16_t raw_data    = encoder_read_raw();
        uint16_t angle_count  = (uint16_t)(raw_data >> ENC_ANGLE_SHIFT);
        float    angle_mech     = (float)angle_count * ENC_TWO_PI / ENC_COUNTS_PER_REV;
        float    angle     = angle_mech * (float)ENC_POLE_PAIRS;
        float    offset_hint;

        while (angle >= ENC_TWO_PI) { angle -= ENC_TWO_PI; }

        /* 预定位时电压矢量加在 FOC_ALIGN_THETA + 90° 方向，
           转子 d 轴会停在那里。所以 offset 应该让
               (angle_mech × 极对数 + offset) 等于 FOC_ALIGN_THETA + π/2 */
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
      /* 速度闭环：目标转速 / 实测转速 / 速度误差 / voltage_q / 三相占空比 */
      vofa_channel[0] = g_foc_dbg.target;            /* 目标转速 rad/s */
      vofa_channel[1] = g_encoder_dbg.speed;  /* 实测转速 rad/s */
      vofa_channel[2] = g_foc_dbg.error_pos;        /* 速度误差 */
      vofa_channel[3] = g_foc_dbg.voltage_q;
      vofa_channel[4] = g_foc_dbg.duty_a;
      vofa_channel[5] = g_foc_dbg.duty_b;
      vofa_channel[6] = g_foc_dbg.duty_c;
#elif USE_POSITION_LOOP
      /* 位置闭环：目标角 / 实际角 / 误差 / 电压 / 三相占空比 */
      vofa_channel[0] = g_foc_dbg.target;
      vofa_channel[1] = g_foc_dbg.angle_mech;
      vofa_channel[2] = g_foc_dbg.error_pos;
      vofa_channel[3] = g_foc_dbg.voltage_q;
      vofa_channel[4] = g_foc_dbg.duty_a;
      vofa_channel[5] = g_foc_dbg.duty_b;
      vofa_channel[6] = g_foc_dbg.duty_c;
#else
      /* 开环：g_target_speed / 【编码器算出的机械角速度】 / angle / voltage_q / 三相占空比
         ch1 是"编码器读数变化速度"：
           正值 = 编码器读数在增大
           负值 = 编码器读数在减小
         开环给 +3 rad/s 时，ch1 应该读到 +3 左右。
         如果读到 -3，说明编码器方向和磁场方向相反，ENC_DIR 要改成 -1。 */
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

          /* 过零处理：实际转过的角度一定小于半圈 */
          if (delta_angle >  ENC_PI) { delta_angle -= ENC_TWO_PI; }
          if (delta_angle < -ENC_PI) { delta_angle += ENC_TWO_PI; }

          speed_instant = delta_angle / dt;

          /* 合理性检查：这台电机不可能超过 15 rad/s。
             超出的都是编码器偶发跳读，直接丢掉这一拍，不污染滤波值 */
          if (speed_instant > 15.0f || speed_instant < -15.0f)
          {
            speed_instant = s_speed;   /* 用上一次的值顶上 */
          }

          /* 一阶低通，时间常数 0.5 秒，把残余毛刺平均掉 */
          s_speed += (speed_instant - s_speed) * (dt / 0.5f);
        }

        s_last_angle  = angle_now;
        s_last_tick = tick_now;
        speed        = s_speed;

        vofa_channel[0] = g_target_speed;
        vofa_channel[1] = speed;
        /* ch2 = 直接反推出来的电角度偏移：磁场角 − 极对数 × 机械角
           开环慢速转、转子不丢步时，这个值应该是一个**常数**，
           那个常数就是正确的 ENC_ELEC_OFFSET */
        {
          float offset_now = g_foc_dbg.angle
                           - (float)ENC_POLE_PAIRS * angle_now;
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

#if VOFA_USE_TEXT
      /* FireWater：发 "2.000,14.000,6.283,1.640,0.500,0.558,0.442\n"
         优点是人眼/串口助手直接能看懂，调通阶段建议先用这个 */
      (void)vofa_send_text(vofa_channel, 7);
#else
      /* JustFloat：浮点二进制 + 帧尾 00 00 80 7F，带宽省 */
      vofa_begin();
      vofa_put(vofa_channel[0]);
      vofa_put(vofa_channel[1]);
      vofa_put(vofa_channel[2]);
      vofa_put(vofa_channel[3]);
      vofa_put(vofa_channel[4]);
      vofa_put(vofa_channel[5]);
      vofa_put(vofa_channel[6]);
      (void)vofa_end();              /* 忙就丢帧，不阻塞电机控制 */
#endif
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
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

  /** Initializes the CPU, AHB and APB buses clocks
  */
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

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
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
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
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
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(cs_MT6701_GPIO_Port, cs_MT6701_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : cs_MT6701_Pin */
  GPIO_InitStruct.Pin = cs_MT6701_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(cs_MT6701_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/* TIM1 更新中断：覆盖启动文件里的弱定义 */
void TIM1_UP_TIM10_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&htim1);
}

/* HAL 的更新事件回调，20kHz 调一次 FOC */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM1)
  {
#if ENC_TEST_MODE == 3
    foc_static(FOC_ALIGN_THETA, g_cal_voltage);
#elif ENC_TEST_MODE == 4
    foc_close_position(g_target_angle);   /* offset 扫描时位置环照常跑 */
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

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
