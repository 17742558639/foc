/* ============================================================
 *  Vofa.c  ——  VOFA+ JustFloat 协议波形发送（USB 虚拟串口）
 * ------------------------------------------------------------
 *  一帧长这样：
 *
 *      [float ch0][float ch1]...[float chN-1]   00 00 80 7F
 *      └────────── 每个 4 字节，小端 ─────────┘  └── 帧尾 ──┘
 *
 *  STM32 是小端，所以直接把 float 的 4 个字节抄进去就是对的，
 *  不用做任何字节交换。
 *
 *  为什么发送放在主循环：CDC_Transmit_FS 会读改 USB 协议栈的内部
 *  状态，若在 20kHz 的 FOC 中断里调用，可能和 OTG_FS 中断打架。
 *  这里用 PRIMASK 短暂关中断，只保护"检查 + 启动发送"这一小段。
 * ============================================================ */

#include "Vofa.h"
#include "main.h"           /* 拿到 __get_PRIMASK / __disable_irq */
#include "usb_device.h"     /* MX_USB_DEVICE_Init 的原型 */
#include "usbd_core.h"
#include "usbd_cdc_if.h"
#include <string.h>

/* USB 设备句柄，定义在 usb_device.c */
extern USBD_HandleTypeDef hUsbDeviceFS;

#define VOFA_TAIL_LEN    4u
#define VOFA_FRAME_MAX   (VOFA_MAX_CH * 4u + VOFA_TAIL_LEN)

/* ============================================================
 *  整个模块的状态打包成一个结构体，本文件只认这一个实例
 * ============================================================ */
typedef struct
{
    uint8_t  frame[VOFA_FRAME_MAX];     /* 待发送的数据（含帧尾） */
    uint8_t  count;                     /* 本帧已经攒了几个通道 */
    uint16_t send_ok;                   /* 发送成功次数（查问题用） */
    uint16_t send_busy;                 /* 丢帧次数（查问题用） */
} vofa_dev_t;

static vofa_dev_t s_vofa;

/* JustFloat 帧尾：00 00 80 7F（浮点 +∞） */
static const uint8_t s_tail[VOFA_TAIL_LEN] = { 0x00, 0x00, 0x80, 0x7F };


/* ============================================================
 *  初始化：只清状态（USB 设备由 main.c 启动）
 * ============================================================ */
void vofa_init(void)
{
    s_vofa.count     = 0;
    s_vofa.send_ok   = 0;
    s_vofa.send_busy = 0;

    /* main.c 里本来就有 CubeMX 生成的 MX_USB_DEVICE_Init()。
       但那个文件是自动生成的，万一重新生成时这行丢了，USB 就永远起不来。
       这里做个兜底：pClass 为空说明还没初始化过，才补一次；
       已经初始化过的绝不能再初始化，否则电脑上的串口会掉线。 */
    if (hUsbDeviceFS.pClass == NULL)
    {
        MX_USB_DEVICE_Init();
    }
}


/* ============================================================
 *  清空本帧
 * ============================================================ */
void vofa_begin(void)
{
    s_vofa.count = 0;
}


/* ============================================================
 *  追加一个通道
 * ============================================================ */
void vofa_put(float value)
{
    if (s_vofa.count >= VOFA_MAX_CH)
    {
        return;                     /* 超了就丢掉，不让它越界写 */
    }

    memcpy(&s_vofa.frame[s_vofa.count * 4u], &value, 4u);
    s_vofa.count++;
}


/* ============================================================
 *  打包并发送本帧
 * ============================================================ */
uint8_t vofa_end(void)
{
    uint16_t len;
    uint8_t  result;
    uint32_t primask;

    if (s_vofa.count == 0)
    {
        return VOFA_ERR_ARG;
    }

    len = (uint16_t)(s_vofa.count * 4u);

    /* 加上 4 字节帧尾 */
    memcpy(&s_vofa.frame[len], s_tail, VOFA_TAIL_LEN);
    len = (uint16_t)(len + VOFA_TAIL_LEN);

    s_vofa.count = 0;

    /* USB 还没枚举完（电脑上还没出现串口）就直接跳过，
       否则协议栈里的 pClassData 还是空指针，一碰就 HardFault */
    if ((hUsbDeviceFS.pClassData == NULL) ||
        (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED))
    {
        s_vofa.send_busy++;
        return VOFA_BUSY;
    }

    /* 保护"检查忙 + 启动发送"这一段，防止 OTG_FS 中断插进来 */
    primask = __get_PRIMASK();
    __disable_irq();
    result = CDC_Transmit_FS(s_vofa.frame, len);
    __set_PRIMASK(primask);

    if (result == USBD_OK)
    {
        s_vofa.send_ok++;
        return VOFA_OK;
    }

    s_vofa.send_busy++;
    return VOFA_BUSY;               /* 上一帧还没发完，本帧丢弃 */
}


/* ============================================================
 *  一次性发送一个数组
 * ============================================================ */
uint8_t vofa_send(const float *data, uint8_t channels)
{
    uint8_t i;

    if ((data == NULL) || (channels == 0) || (channels > VOFA_MAX_CH))
    {
        return VOFA_ERR_ARG;
    }

    vofa_begin();
    for (i = 0; i < channels; i++)
    {
        vofa_put(data[i]);
    }

    return vofa_end();
}


/* ============================================================
 *  FireWater 文本协议（VOFA+ 的另一种数据引擎）
 * ------------------------------------------------------------
 *  格式："任意名字:数值,数值,...,数值\n"
 *    - 冒号和名字可以不要，但【换行不能少】，VOFA+ 收到 \n 才开始解析
 *    - 通道之间用【逗号】分隔，注意逗号后面不要加空格也行，加了也行
 *    - 好处：用任何串口助手都能看到明文，一眼就知道通没通
 *    - 坏处：字符串比二进制占带宽，200Hz×7 通道约 9KB/s，USB 绰绰有余
 *
 *  这里不用 sprintf("%f")：一是会把整个浮点格式化库链接进来（几 KB），
 *  二是慢。手写一个“整数部分 + 3 位小数”的转换，20 行搞定。
 * ============================================================ */

/* 把 float 写成 "-12.345" 这样的定长 3 位小数，返回写了几个字符 */
static uint8_t vofa_put_fixed3(char *dst, float value)
{
    uint8_t  n = 0;
    int32_t  whole;
    uint32_t frac;
    char     digits[10];
    uint8_t  k = 0;

    if (value < 0.0f)
    {
        dst[n++] = '-';
        value = -value;
    }

    whole = (int32_t)value;

    /* 小数部分四舍五入到 0.001 */
    frac = (uint32_t)((value - (float)whole) * 1000.0f + 0.5f);
    if (frac >= 1000u)
    {
        frac -= 1000u;
        whole++;
    }

    /* 整数部分：倒着取位，再翻回来 */
    if (whole == 0)
    {
        digits[k++] = '0';
    }
    while (whole > 0)
    {
        digits[k++] = (char)('0' + (whole % 10));
        whole /= 10;
    }
    while (k > 0)
    {
        dst[n++] = digits[--k];
    }

    /* 小数点后固定 3 位 */
    dst[n++] = '.';
    dst[n++] = (char)('0' + (frac / 100u));
    dst[n++] = (char)('0' + ((frac / 10u) % 10u));
    dst[n++] = (char)('0' + (frac % 10u));

    return n;
}


uint8_t vofa_send_text(const float *data, uint8_t channels)
{
    static char s_text[VOFA_MAX_CH * 14u + 2u];
    uint16_t len = 0;
    uint8_t  i;
    uint8_t  result;
    uint32_t primask;

    if ((data == NULL) || (channels == 0) || (channels > VOFA_MAX_CH))
    {
        return VOFA_ERR_ARG;
    }

    if ((hUsbDeviceFS.pClassData == NULL) ||
        (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED))
    {
        s_vofa.send_busy++;
        return VOFA_BUSY;
    }

    for (i = 0; i < channels; i++)
    {
        len = (uint16_t)(len + vofa_put_fixed3(&s_text[len], data[i]));
        if ((i + 1u) < channels)
        {
            s_text[len++] = ',';
        }
    }
    s_text[len++] = '\n';           /* 换行是 FireWater 的帧结束标志，不能省 */

    primask = __get_PRIMASK();
    __disable_irq();
    result = CDC_Transmit_FS((uint8_t *)s_text, len);
    __set_PRIMASK(primask);

    if (result == USBD_OK)
    {
        s_vofa.send_ok++;
        return VOFA_OK;
    }

    s_vofa.send_busy++;
    return VOFA_BUSY;
}


/* ============================================================
 *  读统计：波形不动时可以看这两个数判断卡在哪
 *    ok 一直不涨  → USB 没枚举 / 没进主循环
 *    busy 涨得飞快 → 采样率太高，USB 跟不上
 * ============================================================ */
void vofa_get_stats(uint16_t *ok_count, uint16_t *busy_count)
{
    if (ok_count   != NULL) { *ok_count   = s_vofa.send_ok;   }
    if (busy_count != NULL) { *busy_count = s_vofa.send_busy; }
}
