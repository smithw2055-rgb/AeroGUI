from PIL import Image

b = Image.open(r"C:\Users\macx\.gemini\antigravity-ide\brain\6802036f-36a1-4baf-877e-95c5c9ae18b9\.user_uploaded\media_1789345404377.png")
# "START GAME" text is white/near-white. In the button area:
# In benchmark: let's find the small orange/blue caret next to START GAME:
# Look at the blue triangle at the left of START GAME:
w, h = b.size
for y in range(160, 220):
    for x in range(180, 240):
        r, g, blue, a = b.getpixel((x, y))
        if r < 30 and g > 180 and blue > 220:
            print(f"Benchmark START caret at ({x}, {y})")
            break

r_im = Image.open(r"C:\Projects\AeroGUI-R\build\menu3d_rendered.png")
for y in range(250, 320):
    for x in range(400, 500):
        r, g, blue, a = r_im.getpixel((x, y))
        if r < 30 and g > 180 and blue > 220:
            print(f"Rendered START caret at ({x}, {y})")
            break
