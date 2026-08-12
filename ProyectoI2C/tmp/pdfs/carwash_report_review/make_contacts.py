from pathlib import Path

from PIL import Image, ImageDraw


folder = Path(__file__).parent
pages = sorted(folder.glob("page-*.png"))

for group_start in range(0, len(pages), 4):
    subset = pages[group_start : group_start + 4]
    canvas = Image.new("RGB", (1040, 1360), "#b8b8b8")
    draw = ImageDraw.Draw(canvas)

    for index, path in enumerate(subset):
        page = Image.open(path).convert("RGB")
        page.thumbnail((500, 640))
        x = 10 + (index % 2) * 520
        y = 30 + (index // 2) * 670
        canvas.paste(page, (x, y))
        draw.text((x, y - 20), f"Pagina {group_start + index + 1}", fill="black")

    canvas.save(folder / f"contact-{group_start // 4 + 1:02d}.png")

print(f"{len(pages)} pages, {(len(pages) + 3) // 4} contact sheets")
