from PIL import Image

# In crop_benchmark: "START GAME"
# Let's inspect the text pixels of "START" in benchmark vs aerogui old
b = Image.open(r"C:\Projects\AeroGUI-R\build\crop_benchmark.png")
old = Image.open(r"C:\Projects\AeroGUI-R\build\crop_aerogui_old.png")

# Let's find white pixels of the word "START" in both
def find_text_bounds(im, x_range, y_range):
    min_x, max_x = im.width, 0
    min_y, max_y = im.height, 0
    for y in range(y_range[0], y_range[1]):
        for x in range(x_range[0], x_range[1]):
            r, g, blue, a = im.getpixel((x, y))
            # near white
            if r > 200 and g > 200 and blue > 200:
                min_x = min(min_x, x)
                max_x = max(max_x, x)
                min_y = min(min_y, y)
                max_y = max(max_y, y)
    return (min_x, min_y, max_x, max_y)

print("Benchmark 'START GAME':", find_text_bounds(b, (20, 150), (10, 60)))
print("Old AeroGUI 'START GAME':", find_text_bounds(old, (20, 150), (10, 60)))
