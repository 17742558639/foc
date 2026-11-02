import struct, numpy as np
raw = open(r"D:\PMSMfoc\debug\com8_capture.bin","rb").read()
TAIL=b"\x00\x00\x80\x7f"
ends=[]; i=raw.find(TAIL)
while i!=-1: ends.append(i); i=raw.find(TAIL,i+1)
starts=[e-28 for e in ends]
frames=[]
for k in range(len(starts)-1):
    if starts[k]>=0 and starts[k+1]-starts[k]==32:
        frames.append(raw[starts[k]:starts[k]+32])
d=np.array([struct.unpack("<7f",f[:28]) for f in frames])
th=d[:,2]
dt=np.diff(th)
print("frames",len(d))
print("delta theta: median %.4f  mean %.4f  min %.4f  max %.4f"%(np.median(dt),dt.mean(),dt.min(),dt.max()))
big=np.where(dt<-1.0)[0]
print("negative jumps:",len(big),"at",big[:20])
print("delta samples around jump:",[round(float(dt[j]),3) for j in big[:5]])
# 检查每帧是否都来自同一个连续流（theta 差分应该几乎恒定）
print("std of positive deltas: %.5f"%np.std(dt[dt>0]))
# duty 三相差 120 度？
for c in range(4,7):
    print("ch%d mean %.4f"%(c,d[:,c].mean()))
