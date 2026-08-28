from PIL import Image
import random

input_file = "glock.png"
output_file = "glock1.png"

STRENGTH = 0.8

img = Image.open(input_file).convert("RGB")
pixels = img.load()

for y in range(img.height):
    for x in range(img.width):
        r, g, b = pixels[x, y]

        # Random float between -STRENGTH and +STRENGTH
        noise = random.uniform(-STRENGTH, STRENGTH)

        pixels[x, y] = (
            max(0, min(255, round(r + noise))),
            max(0, min(255, round(g + noise))),
            max(0, min(255, round(b + noise))),
        )

img.save(output_file)

print(f"Saved: {output_file}")