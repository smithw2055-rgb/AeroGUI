from PIL import Image

b = Image.open(r"C:\Users\macx\.gemini\antigravity-ide\brain\6802036f-36a1-4baf-877e-95c5c9ae18b9\.user_uploaded\media_1789345404377.png")
w, h = b.size
min_x, max_x = w, 0
min_y, max_y = h, 0

for y in range(40, 160):
    for x in range(w):
        r, g, blue, a = b.getpixel((x, y))
        # Blue logo
        if r < 60 and 140 < g < 180 and 210 < blue < 240:
            min_x = min(min_x, x)
            max_x = max(max_x, x)
            min_y = min(min_y, y)
            max_y = max(max_y, y)

print(f"Benchmark Top Blue Logo bounds: ({min_x}, {min_y}) to ({max_x}, {max_y})")

r_im = Image.open(r"C:\Projects\AeroGUI-R\build\menu3d_rendered.png")
w, h = r_im.size
min_x, max_x = w, 0
min_y, max_y = h, 0
for y in range(40, 240):
    for x in range(w):
        r, g, blue, a = r_im.getpixel((x, y))
        if r < 60 and 140 < g < 180 and 210 < blue < 240:
            min_x = min(min_x, x)
            max_x = max(max_x, x)
            min_y = min(min_y, y)
            max_y = max(max_y, y)
print(f"Rendered Top Blue Logo bounds: ({min_x}, {min_y}) to ({max_x}, {max_y})")
