from PIL import Image

b = Image.open(r"C:\Projects\AeroGUI-R\build\crop_benchmark.png")
old = Image.open(r"C:\Projects\AeroGUI-R\build\crop_aerogui_old.png")

# Let's inspect the first letter 'S' in both
# In b:
# Find bounds of 'S' in b
for x in range(30, 60):
    col = [b.getpixel((x, y))[0] > 200 for y in range(25, 65)]
    if any(col):
        ys = [25 + i for i, val in enumerate(col) if val]
        # print(f"b x={x}: y from {min(ys)} to {max(ys)}, h={max(ys)-min(ys)+1}")

# S in benchmark: y is around 32 to 55 -> height is ~23 pixels.
# Total width of "START GAME" in benchmark:
print("Let's measure full width of 'START GAME':")
# In benchmark:
xs_b = [x for x in range(30, 200) if any(b.getpixel((x, y))[0] > 180 for y in range(25, 65))]
print(f"Benchmark 'START GAME' x: {min(xs_b)} to {max(xs_b)}, width = {max(xs_b) - min(xs_b) + 1}")

xs_old = [x for x in range(30, 200) if any(old.getpixel((x, y))[0] > 180 for y in range(5, 30))]
print(f"Old AeroGUI 'START GAME' x: {min(xs_old)} to {max(xs_old)}, width = {max(xs_old) - min(xs_old) + 1}")

ys_b = [y for y in range(25, 65) if any(b.getpixel((x, y))[0] > 180 for x in range(30, 200))]
print(f"Benchmark height: {min(ys_b)} to {max(ys_b)}, h = {max(ys_b) - min(ys_b) + 1}")

ys_old = [y for y in range(5, 30) if any(old.getpixel((x, y))[0] > 180 for x in range(30, 200))]
print(f"Old AeroGUI height: {min(ys_old)} to {max(ys_old)}, h = {max(ys_old) - min(ys_old) + 1}")
