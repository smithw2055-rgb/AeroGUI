import os
from PIL import Image

p1 = r"C:\Users\macx\.gemini\antigravity-ide\brain\6802036f-36a1-4baf-877e-95c5c9ae18b9\.user_uploaded\media_1789345345080.png"
p2 = r"C:\Users\macx\.gemini\antigravity-ide\brain\6802036f-36a1-4baf-877e-95c5c9ae18b9\.user_uploaded\media_1789345404377.png"

im1 = Image.open(p1)
im2 = Image.open(p2)

# In im2 (benchmark): let's find the bounding box of "START GAME"
# Let's crop around x: 200..600, y: 140..250
crop1 = im1.crop((200, 140, 550, 240))
crop2 = im2.crop((200, 140, 550, 240))

crop1.save(r"C:\Projects\AeroGUI-R\build\crop_aerogui_old.png")
crop2.save(r"C:\Projects\AeroGUI-R\build\crop_benchmark.png")
print("Saved crops")
