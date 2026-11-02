from PIL import Image
src = r"C:\Users\m'm'm\.dsh\attachments\v1\objects\b2\b20037003702ac2efdde8daf6da2046a38970bb28e42fcf77b3d74911d2dc6f8"
im = Image.open(src)
box = (1040, 400, 1279, 1650)
crop = im.crop(box).rotate(90, expand=True)
crop = crop.resize((crop.width*4, crop.height*4), Image.LANCZOS)
crop.save(r"D:\PMSMfoc\debug\enc_text2.png")
print("saved", crop.size)
