from PIL import Image
src = r"C:\Users\m'm'm\.dsh\attachments\v1\objects\b2\b20037003702ac2efdde8daf6da2046a38970bb28e42fcf77b3d74911d2dc6f8"
im = Image.open(src)
# 右侧丝印整块，原图坐标
box = (1050, 480, 1279, 1150)
crop = im.crop(box)
crop = crop.rotate(-90, expand=True)          # 转正
crop = crop.resize((crop.width*4, crop.height*4), Image.LANCZOS)
crop.save(r"D:\PMSMfoc\debug\enc_text.png")
print("saved", crop.size)
