# -*- coding: utf-8 -*-
"""从 COM8 抓到的 JustFloat 原始字节里解出 7 个通道，并画成波形图。"""
import struct
import numpy as np
from PIL import Image, ImageDraw, ImageFont

BIN = r'D:\PMSMfoc\debug\com8_capture.bin'
OUT = r'D:\PMSMfoc\debug\vofa_waveform.png'
TAIL = b'\x00\x00\x80\x7f'

raw = open(BIN, 'rb').read()

# 1) 找出所有帧尾的位置
ends = []
i = raw.find(TAIL)
while i != -1:
    ends.append(i)
    i = raw.find(TAIL, i + 1)

# 2) 帧尾前 28 字节 = 7 个 float；只保留间距正好 32 的连续帧
starts = [e - 28 for e in ends]
frames = []
for k in range(len(starts) - 1):
    if starts[k] >= 0 and starts[k + 1] - starts[k] == 32:
        frames.append(raw[starts[k]:starts[k] + 32])

data = np.array([struct.unpack('<7f', f[:28]) for f in frames], dtype=float)
n = len(data)
t = np.arange(n) * 5.0            # ms

print('raw bytes      :', len(raw))
print('tail count     :', len(ends))
print('good frames    :', n)
print('time span      : %.2f s' % (n * 0.005))

names = ['w_cmd', 'we', 'theta', 'duty_a', 'duty_b', 'duty_c', 'uq']
idx = {'w_cmd': 0, 'we': 1, 'theta': 2, 'uq': 3, 'duty_a': 4, 'duty_b': 5, 'duty_c': 6}
for nm in names:
    c = data[:, idx[nm]]
    print('%-8s min=%9.4f max=%9.4f mean=%9.4f' % (nm, c.min(), c.max(), c.mean()))

theta = data[:, 2]
wraps = int(np.sum(np.diff(theta) < -1.0))
print('theta wraps    :', wraps, '=> 电频率 %.3f Hz' % (wraps / (n * 0.005)))

duty = data[:, 4:7]
print('占空比之和 min=%.3f max=%.3f' % (duty.sum(axis=1).min(), duty.sum(axis=1).max()))

# ---------------- 画图 ----------------
W, H = 1400, 1080
img = Image.new('RGB', (W, H), 'white')
d = ImageDraw.Draw(img)
try:
    font = ImageFont.truetype(r'C:\Windows\Fonts\msyh.ttc', 18)
    fbig = ImageFont.truetype(r'C:\Windows\Fonts\msyh.ttc', 26)
except Exception:
    font = ImageFont.load_default()
    fbig = font

X0, X1 = 120, 1340
d.text((X0, 18), 'COM8 实时抓取（5 秒 / 1001 帧 / 200 Hz）— JustFloat 解码', fill=(20, 20, 20), font=fbig)


def panel(y0, h, series, title, colors, width=2):
    d.rectangle([X0, y0, X1, y0 + h], outline=(170, 170, 170))
    allv = np.concatenate([s for s in series])
    vmin, vmax = float(allv.min()), float(allv.max())
    if vmax - vmin < 1e-6:
        vmin, vmax = vmin - 1, vmax + 1
    pad = (vmax - vmin) * 0.12
    vmin -= pad
    vmax += pad
    for g in range(5):
        yy = y0 + h * g / 4.0
        d.line([X0, yy, X1, yy], fill=(232, 232, 232))
        d.text((X0 - 92, yy - 10), '%7.3f' % (vmax - (vmax - vmin) * g / 4.0), fill=(90, 90, 90), font=font)
    for s, col in zip(series, colors):
        pts = [(X0 + (X1 - X0) * k / max(n - 1, 1), y0 + h * (vmax - s[k]) / (vmax - vmin)) for k in range(n)]
        d.line(pts, fill=col, width=width)
    d.text((X0, y0 - 26), title, fill=(20, 20, 20), font=font)


panel(80, 190, [theta], '通道2  theta 电角度 (rad) — 应该是锯齿波，周期约 449ms', [(220, 40, 190)])
panel(340, 190, [data[:, 4], data[:, 5], data[:, 6]],
      '通道4~6  三相占空比 (0~1) — 三路正弦，互相差 120°',
      [(230, 30, 60), (30, 160, 60), (40, 90, 220)])
panel(600, 190, [data[:, 1], data[:, 0]],
      '通道1 we 电角速度 (红) 与 通道0 w_cmd 给定 (蓝)，单位 rad/s',
      [(230, 30, 60), (40, 90, 220)])
panel(860, 150, [data[:, 3]], '通道3  uq q轴电压 (V) — 低通看趋势，正弦纹波是正常的',
      [(200, 120, 20)])

d.text((X0, H - 32), '横轴：0 ~ 5 s（每点 5 ms）', fill=(90, 90, 90), font=font)
img.save(OUT)
print('saved:', OUT)
