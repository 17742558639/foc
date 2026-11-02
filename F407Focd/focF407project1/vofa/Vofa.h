/* ============================================================
 *  Vofa.h  ——  VOFA+ JustFloat 协议波形发送
 * ------------------------------------------------------------
 *  协议：一帧 = N 个 float（小端） + 4 字节帧尾 00 00 80 7F
 *        帧尾就是浮点 +∞，VOFA+ 靠它切帧。
 *
 *  通道：USB CDC 虚拟串口（板上 Type-C，PA11/PA12），
 *        电脑上会多出一个 COM 口，VOFA+ 里选它 + JustFloat。
 *
 *  用法：
 *      vofa_init();                  // main 里初始化一次
 *      ...
 *      vofa_begin();                 // 主循环里，每帧先清空
 *      vofa_put(w_cmd);              // 通道 0
 *      vofa_put(uq);                 // 通道 1 ...
 *      vofa_end();                   // 打包发送
 * ============================================================ */
#ifndef __VOFA_H__
#define __VOFA_H__

#include <stdint.h>

/* 一帧最多几个通道。8 个够看转速/角度/电压/三相占空比 */
#define VOFA_MAX_CH     8

/* vofa_end() / vofa_send() 的返回值 */
#define VOFA_OK         0       /* 发送成功（已交给 USB） */
#define VOFA_BUSY       1       /* USB 还没发完上一帧，本帧被丢弃 */
#define VOFA_ERR_ARG    2       /* 通道数为 0 或超过 VOFA_MAX_CH */

/* 初始化：清空帧计数。USB 设备本身由 main.c 里 CubeMX 生成的
 * MX_USB_DEVICE_Init() 启动，这里不要重复初始化。 */
void vofa_init(void);

/* 清空本帧的通道计数 */
void vofa_begin(void);

/* 追加一个通道。超过 VOFA_MAX_CH 的部分会被忽略 */
void vofa_put(float value);

/* 打包并发送本帧。返回 VOFA_OK / VOFA_BUSY / VOFA_ERR_ARG */
uint8_t vofa_end(void);

/* 一次性发送一个数组，不用 begin/put/end 那一套 */
uint8_t vofa_send(const float *data, uint8_t channels);

/* FireWater 文本协议：把 data[0..channels-1] 写成 "1.234,5.678\n" 发出去。
 * VOFA+ 里数据引擎要选 FireWater；也能用任何串口助手直接看到明文。 */
uint8_t vofa_send_text(const float *data, uint8_t channels);

/* 读发送统计（查问题时用，不需要可不管） */
void vofa_get_stats(uint16_t *ok_count, uint16_t *busy_count);

#endif
