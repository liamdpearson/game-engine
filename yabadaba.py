from PIL import Image
import random

input_file = "old.png"
output_file = "new.png"

noise_amount = 2  # Maximum change per pixel

img = Image.open(input_file).convert("RGB")
pixels = img.load()

for y in range(img.height):
    for x in range(img.width):
        r, g, b = pixels[x, y]

        if (r + g + b)/3 < 100:
            continue

        # One noise value shared by all RGB channels
        noise = random.randint(-noise_amount, noise_amount)

        r = max(0, min(255, r + noise))
        g = max(0, min(255, g + noise))
        b = max(0, min(255, b + noise))

        pixels[x, y] = (r, g, b)

img.save(output_file)