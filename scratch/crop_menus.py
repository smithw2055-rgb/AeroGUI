from PIL import Image

b = Image.open(r"C:\Users\macx\.gemini\antigravity-ide\brain\6802036f-36a1-4baf-877e-95c5c9ae18b9\.user_uploaded\media_1789345404377.png")
print("Benchmark size:", b.size)

# Crop the whole menu area in benchmark (around x: 100..700, y: 50..450)
crop_b = b.crop((100, 50, 700, 450))
crop_b.save(r"C:\Projects\AeroGUI-R\build\crop_benchmark_menu.png")

# Now crop the same area in our rendered image
r = Image.open(r"C:\Projects\AeroGUI-R\build\menu3d_rendered.png")
print("Rendered size:", r.size)
crop_r = r.crop((100, 50, 700, 450))
crop_r.save(r"C:\Projects\AeroGUI-R\build\crop_rendered_menu.png")
