import os
from PIL import Image

p1 = r"C:\Users\macx\.gemini\antigravity-ide\brain\6802036f-36a1-4baf-877e-95c5c9ae18b9\.user_uploaded\media_1789345345080.png"
p2 = r"C:\Users\macx\.gemini\antigravity-ide\brain\6802036f-36a1-4baf-877e-95c5c9ae18b9\.user_uploaded\media_1789345404377.png"

for p in [p1, p2]:
    if os.path.exists(p):
        im = Image.open(p)
        print(f"{os.path.basename(p)}: size={im.size}, mode={im.mode}")
