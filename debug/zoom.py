from PIL import Image
src = r"C:\Users\m'm'm\.dsh\attachments\v1\objects\b2\b20037003702ac2efdde8daf6da2046a38970bb28e42fcf77b3d74911d2dc6f8"
im = Image.open(src)
print("size", im.size)
crop = im.crop((450, 300, 1250, 1000)).resize((1600, 1400), Image.LANCZOS)
crop.save(r"D:\PMSMfoc\debug\enc_zoom.png")
print("saved")
