"""生成 FLFHMCLauncher 图标：HMCL 图标 + 右下角 Fabric 徽标。

用法:
    python assets/icon/generate_icon.py
"""

from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
HMCL_ICON = Path(r"K:/FLFHMCL/HMCL/HMCL/src/main/resources/assets/img/icon@8x.png")
FABRIC_ICON = Path(r"K:/FLFHMCL/HMCL/HMCL/src/main/resources/assets/img/fabric@2x.png")
OUT = ROOT / "assets" / "icon"

SIZE = 256
BADGE_SIZE = 88
BADGE_MARGIN = 3


def main() -> None:
    base = Image.open(HMCL_ICON).convert("RGBA")
    if base.size != (SIZE, SIZE):
        base = base.resize((SIZE, SIZE), Image.LANCZOS)

    fabric = Image.open(FABRIC_ICON).convert("RGBA")
    fabric = fabric.resize((BADGE_SIZE - 20, BADGE_SIZE - 20), Image.LANCZOS)

    # 白色圆形底，保证 Fabric 徽标在草地纹理上可辨认
    badge = Image.new("RGBA", (BADGE_SIZE, BADGE_SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(badge)
    draw.ellipse((0, 0, BADGE_SIZE - 1, BADGE_SIZE - 1), fill=(255, 255, 255, 235),
                 outline=(40, 40, 40, 220), width=3)

    offset = ((BADGE_SIZE - fabric.width) // 2, (BADGE_SIZE - fabric.height) // 2)
    badge.alpha_composite(fabric, offset)

    position = (SIZE - BADGE_SIZE - BADGE_MARGIN, SIZE - BADGE_SIZE - BADGE_MARGIN)
    base.alpha_composite(badge, position)

    OUT.mkdir(parents=True, exist_ok=True)
    for size in (256, 128, 64, 48, 32, 16):
        base.resize((size, size), Image.LANCZOS).save(OUT / f"flfhmlauncher-{size}.png")

    base.save(OUT / "flfhmlauncher.png")
    base.save(
        OUT / "flfhmlauncher.ico",
        sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)],
    )
    print("generated:", OUT)


if __name__ == "__main__":
    main()

