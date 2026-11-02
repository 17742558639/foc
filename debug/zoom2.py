from PIL import Image, ImageDraw
src = r"C:\Users\m'm'm\.dsh\attachments\v1\objects\b2\b20037003702ac2efdde8daf6da2046a38970bb28e42fcf77b3d74911d2dc6f8"
im = Image.open(src)
box = (540, 820, 940, 1080)
crop = im.crop(box).resize(((box[2]-box[0])*6, (box[3]-box[1])*6), Image.LANCZOS)
crop.save(r"D:\PMSMfoc\debug\r4_zoom.png")
print("saved", crop.size)
