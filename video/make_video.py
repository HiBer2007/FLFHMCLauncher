# -*- coding: utf-8 -*-
"""FLFHMCL 88 秒介绍视频渲染器（Pillow 逐帧 + ffmpeg）。"""
import math
import os
import random
import subprocess
import wave
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

W, H = 1920, 1080
FPS = 30
DURATION = 88.0
ROOT = Path(__file__).resolve().parent
OUT = ROOT / "frames"
OUT.mkdir(parents=True, exist_ok=True)

FONT_CN = r"C:\Windows\Fonts\msyh.ttc"
FONT_CN_B = r"C:\Windows\Fonts\msyhbd.ttc"
FONT_EN = r"C:\Windows\Fonts\segoeuib.ttf"
FONT_MONO = r"C:\Windows\Fonts\consola.ttf"

def font(path, size):
    return ImageFont.truetype(path, size)

F = {
    "h1": font(FONT_CN_B, 92),
    "h2": font(FONT_CN_B, 58),
    "h3": font(FONT_CN_B, 38),
    "body": font(FONT_CN, 30),
    "small": font(FONT_CN, 23),
    "tiny": font(FONT_CN, 18),
    "mono": font(FONT_MONO, 26),
    "mono_s": font(FONT_MONO, 20),
    "num": font(FONT_EN, 64),
}

SCREEN = Path(r"K:\FLFHMCLauncher\build\_dark3.png")
ICON = Path(r"K:\FLFHMCLauncher\assets\icon\flfhmlauncher-256.png")
SCREEN_IMG = Image.open(SCREEN).convert("RGB")
ICON_IMG = Image.open(ICON).convert("RGBA")

BG_TOP = (5, 9, 18)
BG_BOTTOM = (11, 20, 36)
BLUE = (59, 130, 246)
CYAN = (34, 211, 238)
PURPLE = (139, 92, 246)
TEXT = (233, 240, 250)
SUB = (178, 194, 214)
GREEN = (52, 211, 153)
RED = (248, 113, 113)

def clamp(v, a=0.0, b=1.0):
    return max(a, min(b, v))

def smooth(x):
    x = clamp(x)
    return x * x * (3 - 2 * x)

def ease_out(x):
    x = clamp(x)
    return 1 - (1 - x) ** 3

def fade_window(t, start, end, fade=0.5):
    if t < start or t > end:
        return 0.0
    return smooth((t - start) / fade) * smooth((end - t) / fade)

def lerp(a, b, p):
    return a + (b - a) * p

def make_bg():
    img = Image.new("RGB", (W, H), BG_TOP)
    px = img.load()
    for y in range(H):
        p = y / H
        r = int(lerp(BG_TOP[0], BG_BOTTOM[0], p))
        g = int(lerp(BG_TOP[1], BG_BOTTOM[1], p))
        b = int(lerp(BG_TOP[2], BG_BOTTOM[2], p))
        for x in range(W):
            px[x, y] = (r, g, b)
    return img

BG = make_bg()

def radial_glow(radius, color, alpha=120):
    size = radius * 2
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    for r in range(radius, 0, -4):
        p = 1 - r / radius
        a = int(alpha * (p ** 2.2))
        d.ellipse((radius - r, radius - r, radius + r, radius + r),
                  fill=(color[0], color[1], color[2], a))
    return img.filter(ImageFilter.GaussianBlur(18))

GLOW_BLUE = radial_glow(430, BLUE, 150)
GLOW_PURPLE = radial_glow(360, PURPLE, 120)
GLOW_CYAN = radial_glow(300, CYAN, 100)

def paste_glow(base, glow, cx, cy, scale=1.0, alpha=1.0):
    if scale <= 0.01 or alpha <= 0.01:
        return
    g = glow
    if abs(scale - 1.0) > 0.01:
        g = glow.resize((int(glow.width * scale), int(glow.height * scale)))
    if alpha < 1:
        g = g.copy()
        g.putalpha(g.getchannel("A").point(lambda v: int(v * alpha)))
    base.alpha_composite(g, (int(cx - g.width / 2), int(cy - g.height / 2)))

def vgrad(w, h, top, bottom, alpha=255):
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    for y in range(h):
        p = y / max(1, h - 1)
        d.line((0, y, w, y), fill=(int(lerp(top[0], bottom[0], p)),
                                   int(lerp(top[1], bottom[1], p)),
                                   int(lerp(top[2], bottom[2], p)), alpha))
    return img

def draw_text(layer, x, y, s, f, fill, anchor="la", alpha=255):
    if alpha <= 2:
        return
    d = ImageDraw.Draw(layer)
    color = fill if len(fill) == 4 else (*fill, alpha)
    d.text((x, y), s, font=f, fill=color, anchor=anchor)

def text_width(s, f):
    return ImageDraw.Draw(Image.new("RGB", (10, 10))).textlength(s, font=f)

def reveal_text(layer, x, y, s, f, fill, p, anchor="la"):
    p = clamp(p)
    if p <= 0:
        return
    w = int(text_width(s, f) * p) + 4
    tmp = Image.new("RGBA", (max(4, w), f.size + 24), (0, 0, 0, 0))
    ImageDraw.Draw(tmp).text((0, 0), s, font=f, fill=(*fill, 255), anchor="la")
    layer.alpha_composite(tmp, (int(x), int(y - 8)))

def round_rect(d, box, radius, fill=None, outline=None, width=1):
    d.rounded_rectangle(box, radius=radius, fill=fill, outline=outline, width=width)

def draw_grid(layer, t, horizon=760, alpha=42):
    d = ImageDraw.Draw(layer)
    vp = (W // 2, horizon - 220)
    for i in range(-12, 13):
        x = W // 2 + i * 150
        d.line((vp[0], vp[1], x, H + 100), fill=(60, 130, 220, alpha), width=1)
    for i in range(14):
        p = (i + (t * 0.18) % 1) / 14
        y = horizon + int((H - horizon) * (p ** 1.8))
        d.line((0, y, W, y), fill=(60, 130, 220, max(8, int(alpha * (1 - p)))), width=1)

def draw_particles(layer, t, count=110, seed=7, alpha=110):
    rnd = random.Random(seed)
    d = ImageDraw.Draw(layer)
    for i in range(count):
        x = rnd.random() * W
        y = rnd.random() * H
        speed = 8 + rnd.random() * 26
        yy = (y - t * speed) % (H + 40)
        tw = 0.5 + 0.5 * math.sin(t * 2 + i)
        r = 1 + int(rnd.random() * 2)
        d.ellipse((x - r, yy - r, x + r, yy + r),
                  fill=(120, 190, 255, int(alpha * tw)))

def base_scene(t):
    img = BG.convert("RGBA")
    # 呼吸光斑
    paste_glow(img, GLOW_BLUE, 1460 + 80 * math.sin(t * 0.18), 220, 1.0, 0.55)
    paste_glow(img, GLOW_PURPLE, 300 + 60 * math.cos(t * 0.15), 840, 0.9, 0.42)
    draw_grid(img, t)
    draw_particles(img, t)
    # 扫描线
    d = ImageDraw.Draw(img)
    for y in range(0, H, 6):
        d.line((0, y, W, y), fill=(4, 8, 16, 18))
    return img

def vignette(layer):
    mask = Image.new("L", (W, H), 0)
    d = ImageDraw.Draw(mask)
    d.rectangle((0, 0, W, H), fill=255)
    # 边缘压暗
    for i in range(160):
        a = int(180 * (i / 160) ** 2)
        d.rectangle((i, i, W - i, H - i), outline=None)
    edge = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ed = ImageDraw.Draw(edge)
    for i in range(170):
        a = int(160 * (1 - i / 170) ** 2)
        ed.rectangle((i, i, W - i, H - i), outline=(0, 0, 0, a), width=1)
    layer.alpha_composite(edge)

def pill(d, x, y, text, f, fill, text_fill=(255, 255, 255), pad=(18, 8), radius=20, outline=None):
    tw = text_width(text, f)
    box = (x, y, x + tw + pad[0] * 2, y + f.size + pad[1] * 2)
    d.rounded_rectangle(box, radius=radius, fill=fill, outline=outline, width=1)
    d.text((x + pad[0], y + pad[1] - 2), text, font=f, fill=text_fill)
    return box

def scene_intro(layer, t):
    p = fade_window(t, 0.0, 6.3, 0.55)
    if p <= 0:
        return
    d = ImageDraw.Draw(layer)
    # 中心光环
    cx, cy = W // 2, 430
    ring_p = ease_out(clamp((t - 0.2) / 1.1))
    for i in range(3):
        r = int(130 + i * 42 + 16 * math.sin(t * 2 + i))
        a = int(120 * (1 - i / 3) * p)
        d.ellipse((cx - r, cy - r, cx + r, cy + r), outline=(59, 130, 246, a), width=2)
    logo_p = ease_out(clamp((t - 0.15) / 0.8))
    size = int(210 * (0.7 + 0.3 * logo_p))
    icon = ICON_IMG.resize((size, size))
    layer.alpha_composite(icon, (cx - size // 2, cy - size // 2))
    paste_glow(layer, GLOW_BLUE, cx, cy + 10, 0.55, 0.55 * p)

    title = "FLFHMCL"
    f = F["h1"]
    tw = text_width(title, f)
    x = cx - tw / 2
    p2 = smooth((t - 0.75) / 0.8)
    reveal_text(layer, x, cy + 150, title, f, TEXT, p2)
    draw_text(layer, cx, cy + 270, "让 HMCL 跑在 Fabric 之上", F["h2"], SUB,
              anchor="ma", alpha=int(255 * smooth((t - 1.6) / 0.8)))
    draw_text(layer, cx, cy + 345, "跨版本  ·  自包含  ·  可扩展", F["body"], CYAN,
              anchor="ma", alpha=int(255 * smooth((t - 2.2) / 0.8)))

def scene_problem(layer, t):
    p = fade_window(t, 6.0, 15.2, 0.6)
    if p <= 0:
        return
    d = ImageDraw.Draw(layer)
    draw_text(layer, 140, 120, "当 HMCL 版本开始分叉", F["h1"], TEXT,
              alpha=int(255 * p))
    draw_text(layer, 144, 225, "同一个组件，无法同时适配所有 HMCL", F["body"], SUB,
              alpha=int(220 * p))

    cards = [("HMCL 3.14", 300, RED, "API 不兼容"),
             ("HMCL 3.15", 760, (251, 191, 36), "Loader 冲突"),
             ("HMCL 3.16", 1220, RED, "类加载失败")]
    for i, (name, x, color, tag) in enumerate(cards):
        app = smooth((t - 6.5 - i * 0.25) / 0.8)
        if app <= 0:
            continue
        y = 390 + int(28 * math.sin(t * 0.8 + i)) - int((1 - app) * 80)
        box = (x, y, x + 360, y + 250)
        d.rounded_rectangle(box, radius=26, fill=(18, 27, 43, int(235 * app)),
                            outline=(*color, int(170 * app)), width=2)
        draw_text(layer, x + 34, y + 30, name, F["h3"], TEXT, alpha=int(255 * app))
        draw_text(layer, x + 34, y + 88, "X", F["h1"], color, alpha=int(255 * app))
        d.text((x + 110, y + 105), tag, font=F["body"], fill=(*color, int(255 * app)))
        # 裂纹
        d.line((x + 30, y + 190, x + 120, y + 205, x + 210, y + 185, x + 330, y + 205),
               fill=(*color, int(120 * app)), width=2)

    draw_text(layer, W // 2, 760, "每换一个 HMCL 版本，就要重做一次适配", F["h3"], SUB,
              anchor="ma", alpha=int(255 * smooth((t - 9.0) / 0.8)))
    # 扫描警告线
    scan = (t - 8.0) * 0.5 % 1
    if 8 < t < 14:
        y = int(300 + scan * 520)
        d.line((120, y, W - 120, y), fill=(248, 113, 113, 35), width=2)

def card(layer, box, fill=(18, 27, 43, 238), outline=(59, 130, 246, 150), radius=22, width=2):
    d = ImageDraw.Draw(layer)
    d.rounded_rectangle(box, radius=radius, fill=fill, outline=outline, width=width)

def scene_architecture(layer, t):
    p = fade_window(t, 14.6, 25.4, 0.6)
    if p <= 0:
        return
    d = ImageDraw.Draw(layer)
    draw_text(layer, 140, 110, "三层结构，把版本差异关进笼子", F["h2"], TEXT,
              alpha=int(255 * p))

    cx = 760
    layers = [
        ("FLFHMCLauncher", "原生 Qt 启动器 · Provider 选择启动协议", 300, BLUE),
        ("FLFHMCL Agent", "Premain · KnotClassLoader · mclhr", 470, CYAN),
        ("HMCL", "3.14 / 3.15 / 3.16 / 3.17", 640, PURPLE),
    ]
    for i, (name, sub, y, color) in enumerate(layers):
        app = smooth((t - 15.2 - i * 0.35) / 0.7)
        if app <= 0:
            continue
        x = 300 - int((1 - app) * 60)
        box = (x, y, x + 920, y + 120)
        d.rounded_rectangle(box, radius=18, fill=(13, 22, 38, int(240 * app)),
                            outline=(*color, int(180 * app)), width=2)
        d.rounded_rectangle((x, y, x + 8, y + 120), radius=4, fill=(*color, int(255 * app)))
        draw_text(layer, x + 34, y + 22, name, F["h3"], TEXT, alpha=int(255 * app))
        draw_text(layer, x + 34, y + 72, sub, F["small"], SUB, alpha=int(220 * app))
        # 编号
        d.text((x + 840, y + 36), f"0{i+1}", font=F["num"], fill=(*color, int(110 * app)))

    # 连接线与流动点
    for i in range(2):
        y1 = 420 + i * 170
        y2 = 470 + i * 170
        d.line((cx, y1, cx, y2), fill=(59, 130, 246, 160), width=3)
        for k in range(3):
            q = (t * 0.45 + k / 3) % 1
            yy = y1 + (y2 - y1) * q
            d.ellipse((cx - 5, yy - 5, cx + 5, yy + 5), fill=(*CYAN, 235))

    # 右侧注解
    notes = [("KnotClassLoader", 300, 15.8, CYAN),
             ("Mixin 崩溃接管", 470, 17.6, GREEN),
             ("mclhr 库解析", 640, 19.4, PURPLE)]
    for text, y, start, color in notes:
        app = smooth((t - start) / 0.7)
        if app <= 0:
            continue
        nx = 1300
        d.line((nx - 90, y + 55, nx - 20, y + 55), fill=(*color, int(170 * app)), width=2)
        d.ellipse((nx - 26, y + 49, nx - 14, y + 61), fill=(*color, int(255 * app)))
        draw_text(layer, nx, y + 30, text, F["h3"], color, alpha=int(255 * app))
        draw_text(layer, nx, y + 92, "版本差异在这里被吸收", F["small"], SUB,
                  alpha=int(190 * app))

def screenshot_card(layer, t, x, y, w, h, p):
    if p <= 0:
        return
    scale = 0.86 + 0.14 * ease_out(p)
    ww, hh = int(w * scale), int(h * scale)
    shot = SCREEN_IMG.resize((ww, hh))
    # 圆角
    mask = Image.new("L", (ww, hh), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, ww, hh), radius=18, fill=255)
    shadow = Image.new("RGBA", (ww + 80, hh + 80), (0, 0, 0, 0))
    ImageDraw.Draw(shadow).rounded_rectangle((40, 40, ww + 40, hh + 40), radius=24,
                                             fill=(0, 0, 0, 160))
    shadow = shadow.filter(ImageFilter.GaussianBlur(28))
    layer.alpha_composite(shadow, (int(x - 40), int(y - 30)))
    layer.paste(shot, (int(x), int(y)), mask)
    ImageDraw.Draw(layer).rounded_rectangle((x, y, x + ww, y + hh), radius=18,
                                            outline=(59, 130, 246, 170), width=2)

def scene_launcher(layer, t):
    p = fade_window(t, 24.8, 36.2, 0.55)
    if p <= 0:
        return
    # 背景文字
    draw_text(layer, 120, 90, "把复杂留给自己", F["h2"], TEXT, alpha=int(230 * p))
    draw_text(layer, 124, 160, "启动、安装、修复、更新，一个窗口完成", F["body"], SUB,
              alpha=int(190 * p))
    slide = ease_out(clamp((t - 25.2) / 0.9))
    x = int(lerp(1300, 200, slide))
    y = 245
    screenshot_card(layer, t, x, y, 1030, 714, p)

    d = ImageDraw.Draw(layer)
    callouts = [
        ("多实例并行", 1280, 320, 0),
        ("版本独立选择", 1280, 470, 1),
        ("安装进度与重试", 1280, 620, 2),
        ("崩溃报告解析", 1280, 770, 3),
    ]
    for text, cx, cy, i in callouts:
        app = smooth((t - 27.0 - i * 0.35) / 0.6)
        if app <= 0:
            continue
        d.line((cx - 70, cy, cx - 20, cy), fill=(*CYAN, int(190 * app)), width=2)
        d.ellipse((cx - 16, cy - 7, cx - 2, cy + 7), fill=(*CYAN, int(255 * app)))
        tw = text_width(text, F["small"])
        d.rounded_rectangle((cx + 12, cy - 24, cx + 36 + tw, cy + 24), radius=12,
                            fill=(9, 17, 29, int(210 * app)))
        draw_text(layer, cx + 24, cy - 16, text, F["small"], TEXT, alpha=int(245 * app))

def scene_install(layer, t):
    p = fade_window(t, 35.8, 47.6, 0.55)
    if p <= 0:
        return
    draw_text(layer, 120, 86, "安装不是黑盒", F["h2"], TEXT, alpha=int(230 * p))
    draw_text(layer, 124, 154, "每一步都有进度，每个库都有名字", F["body"], SUB,
              alpha=int(190 * p))

    panel = (180, 230, 1740, 930)
    card(layer, panel, fill=(14, 23, 38, 242), outline=(59, 130, 246, 140), radius=24)
    d = ImageDraw.Draw(layer)
    # 左侧总进度
    left = (220, 275, 620, 885)
    d.rounded_rectangle(left, radius=18, fill=(10, 18, 30, 245), outline=(37, 99, 235, 120), width=1)
    draw_text(layer, 250, 305, "安装进度", F["small"], SUB)
    prog = ease_out(clamp((t - 36.3) / 7.0))
    pct = min(100, int(prog * 100))
    d.text((250, 345), f"{pct}%", font=F["num"], fill=TEXT)
    d.rounded_rectangle((250, 450, 590, 468), radius=9, fill=(30, 45, 66, 255))
    d.rounded_rectangle((250, 450, 250 + int(340 * prog), 468), radius=9, fill=BLUE)
    for i, line in enumerate(["准备 Java", "下载 JavaFX", "安装 Fabric", "补全依赖库",
                              "下载 HMCL", "下载 Agent", "写入配置"]):
        yy = 520 + i * 42
        active = pct > (i + 1) * 12
        draw_text(layer, 250, yy, ("OK " if active else "-  ") + line,
                  F["small"], GREEN if active else SUB, alpha=210)

    # 右侧步骤
    steps = ["准备 Java", "下载 JavaFX", "安装 Fabric", "安装依赖库", "下载 HMCL", "下载 FLFHMCL Agent"]
    for i, name in enumerate(steps):
        yy = 272 + i * 102
        active = prog * 6 > i
        done = prog * 6 > i + 1
        d.rounded_rectangle((660, yy, 1700, yy + 86), radius=14,
                            fill=(17, 27, 44, 245) if active else (12, 20, 33, 220),
                            outline=(34, 197, 94, 120) if done else ((59, 130, 246, 110) if active else (30, 45, 66, 120)),
                            width=1)
        d.text((684, yy + 26), "OK" if done else ("..." if active else "-"), font=F["small"],
               fill=GREEN if done else (CYAN if active else SUB))
        draw_text(layer, 724, yy + 20, name, F["small"], TEXT, alpha=235)
        d.text((1600, yy + 22), f"{100 if done else (int(prog * 100) if active else 0)}%",
               font=F["small"], fill=GREEN if done else SUB)
        bw = 760
        d.rounded_rectangle((724, yy + 56, 724 + bw, yy + 62), radius=3, fill=(30, 45, 66, 255))
        local = 1.0 if done else (clamp(prog * 6 - i) if active else 0)
        if local > 0:
            d.rounded_rectangle((724, yy + 56, 724 + int(bw * local), yy + 62), radius=3,
                                fill=GREEN if done else BLUE)
        # 当前大项的子库
        if active and not done and i == 3:
            for k in range(3):
                cy2 = yy + 70 + k * 0
                pass
    # 悬浮子项
    sub_y = 760
    d.rounded_rectangle((700, sub_y, 1680, sub_y + 72), radius=12, fill=(9, 16, 27, 230))
    draw_text(layer, 724, sub_y + 12, "Fabric 库 5/8: net.fabricmc:sponge-mixin", F["tiny"], SUB)
    d.rounded_rectangle((724, sub_y + 45, 724 + 900, sub_y + 51), radius=3, fill=(30, 45, 66, 255))
    d.rounded_rectangle((724, sub_y + 45, 724 + int(900 * prog), sub_y + 51), radius=3, fill=CYAN)

def scene_matrix(layer, t):
    p = fade_window(t, 47.2, 58.6, 0.55)
    if p <= 0:
        return
    draw_text(layer, W // 2, 120, "一套 FLFHMCL，多个 HMCL", F["h1"], TEXT,
              anchor="ma", alpha=int(255 * p))
    draw_text(layer, W // 2, 230, "Agent 负责跨版本，FLF API 负责同版本能力", F["body"], SUB,
              anchor="ma", alpha=int(220 * p))

    versions = ["3.14.0", "3.14.1", "3.15.0", "3.15.3", "3.16.4", "3.17.0"]
    cols = 6
    for i, v in enumerate(versions):
        col = i % cols
        row = i // cols
        x = 250 + col * 250
        y = 380 + row * 190
        app = smooth((t - 48.0 - i * 0.18) / 0.55)
        if app <= 0:
            continue
        ok = v not in ("3.17.0",)
        color = GREEN if ok else (251, 191, 36)
        d = ImageDraw.Draw(layer)
        d.line((x + 105, y + 130, W // 2, 760), fill=(*color, int(55 * app)), width=1)
        d.rounded_rectangle((x, y, x + 210, y + 130), radius=18,
                            fill=(14, 23, 38, int(240 * app)),
                            outline=(*color, int(180 * app)), width=2)
        d.text((x + 24, y + 18), v, font=F["h3"], fill=(*TEXT, int(255 * app)))
        d.text((x + 24, y + 78), "OK 通过" if ok else "... 适配中", font=F["small"],
               fill=(*color, int(255 * app)))

    # 中心徽章
    app = smooth((t - 53.0) / 0.7)
    if app > 0:
        bx, by = W // 2 - 300, 760
        d = ImageDraw.Draw(layer)
        d.rounded_rectangle((bx, by, bx + 600, by + 110), radius=55,
                            fill=(10, 20, 35, int(240 * app)),
                            outline=(*BLUE, int(230 * app)), width=2)
        d.text((bx + 38, by + 25), "FLFHMCL 2.1.6", font=F["h3"], fill=(*TEXT, int(255 * app)))
        d.text((bx + 330, by + 30), "Knot + Mixin", font=F["small"], fill=(*CYAN, int(255 * app)))

def scene_api(layer, t):
    p = fade_window(t, 58.0, 69.4, 0.55)
    if p <= 0:
        return
    draw_text(layer, 120, 100, "FLF API", F["h1"], TEXT, alpha=int(255 * p))
    draw_text(layer, 124, 210, "事件总线、注册表、UI 扩展点", F["body"], SUB, alpha=int(220 * p))

    cx, cy = 1180, 580
    d = ImageDraw.Draw(layer)
    for ring, rad in enumerate((180, 290, 400)):
        d.ellipse((cx - rad, cy - rad, cx + rad, cy + rad),
                  outline=(59, 130, 246, int((80 - ring * 18) * p)), width=2)
    nodes = [("FLF API", -90, BLUE),
             ("UI API", -10, CYAN),
             ("Account", 70, PURPLE),
             ("Download", 150, GREEN),
             ("Modpack", 230, (251, 191, 36))]
    for i, (name, angle, color) in enumerate(nodes):
        app = smooth((t - 59.0 - i * 0.22) / 0.5)
        if app <= 0:
            continue
        a = math.radians(angle + t * 8)
        rad = 300
        x = cx + int(math.cos(a) * rad)
        y = cy + int(math.sin(a) * rad)
        d.line((cx, cy, x, y), fill=(*color, int(90 * app)), width=1)
        d.ellipse((x - 9, y - 9, x + 9, y + 9), fill=(*color, int(255 * app)))
        d.rounded_rectangle((x - 78, y - 70, x + 78, y - 34), radius=16,
                            fill=(13, 23, 39, int(230 * app)),
                            outline=(*color, int(180 * app)), width=1)
        d.text((x, y - 62), name, font=F["tiny"], fill=(*TEXT, int(255 * app)), anchor="ma")

    # 左侧事件流
    events = ["InstanceCreated", "GameLaunching", "DownloadStarted", "CrashHandled"]
    for i, ev in enumerate(events):
        yy = 360 + i * 95
        app = smooth((t - 60.0 - i * 0.28) / 0.5)
        if app <= 0:
            continue
        d.rounded_rectangle((120, yy, 760, yy + 64), radius=14,
                            fill=(13, 23, 39, int(235 * app)),
                            outline=(59, 130, 246, int(120 * app)), width=1)
        d.text((148, yy + 16), ev, font=F["mono_s"], fill=(*CYAN, int(255 * app)))
        d.text((680, yy + 18), "→", font=F["body"], fill=(*SUB, int(220 * app)))

def scene_crash(layer, t):
    p = fade_window(t, 69.0, 79.2, 0.55)
    if p <= 0:
        return
    draw_text(layer, 120, 100, "崩溃也要被接住", F["h2"], TEXT, alpha=int(255 * p))
    draw_text(layer, 124, 170, "Mixin 接管 CrashReporter，报告留在 .hmcl/crash-report", F["body"], SUB,
              alpha=int(210 * p))

    # 终端窗口
    term = (150, 280, 1050, 780)
    d = ImageDraw.Draw(layer)
    d.rounded_rectangle(term, radius=18, fill=(8, 14, 24, int(245 * p)),
                        outline=(139, 92, 246, int(160 * p)), width=2)
    d.rounded_rectangle((150, 280, 1050, 328), radius=18, fill=(20, 30, 48, 255))
    for i, c in enumerate(((248, 113, 113), (251, 191, 36), (52, 211, 153))):
        d.ellipse((176 + i * 26, 296, 190 + i * 26, 310), fill=c)
    lines = [
        "[FLF] CrashReporterMixin invoked",
        "java.lang.IllegalStateException: ...",
        "FLFHMCL crash report saved",
        ".hmcl/crash-report/flfhmcl-crash-2026...txt",
        "process exited with code 1",
    ]
    typed = clamp((t - 70.0) / 4.5)
    for i, line in enumerate(lines):
        if typed * len(lines) < i:
            break
        col = GREEN if i in (0, 2) else (SUB if i != 4 else RED)
        d.text((180, 360 + i * 58), line, font=F["mono_s"], fill=(*col, int(240 * p)))

    # 报告卡片
    app = smooth((t - 72.8) / 0.7)
    if app > 0:
        box = (1130, 330, 1690, 720)
        d.rounded_rectangle(box, radius=20, fill=(14, 23, 38, int(245 * app)),
                            outline=(*GREEN, int(180 * app)), width=2)
        d.text((1160, 360), "crash-report", font=F["h3"], fill=(*TEXT, int(255 * app)))
        d.text((1160, 425), "FLFHMCL: 2.1.6", font=F["mono_s"], fill=(*SUB, int(255 * app)))
        d.text((1160, 470), "Instance: testbed-dev", font=F["mono_s"], fill=(*SUB, int(255 * app)))
        d.text((1160, 515), "Thread: JavaFX Application", font=F["mono_s"], fill=(*SUB, int(255 * app)))
        d.text((1160, 590), "OK 已保存", font=F["body"], fill=(*GREEN, int(255 * app)))
        d.text((1160, 640), "OK 已复制诊断", font=F["body"], fill=(*GREEN, int(255 * app)))

def scene_end(layer, t):
    p = fade_window(t, 78.6, 88.4, 0.6)
    if p <= 0:
        return
    cx, cy = W // 2, 400
    size = 190
    layer.alpha_composite(ICON_IMG.resize((size, size)), (cx - size // 2, cy - size // 2))
    paste_glow(layer, GLOW_BLUE, cx, cy, 0.45, 0.5 * p)
    draw_text(layer, cx, cy + 150, "FLFHMCL", F["h1"], TEXT, anchor="ma",
              alpha=int(255 * p))
    draw_text(layer, cx, cy + 275, "跨版本  ·  自包含  ·  可扩展", F["h2"], CYAN,
              anchor="ma", alpha=int(240 * smooth((t - 80.0) / 0.7)))
    draw_text(layer, cx, cy + 360, "FLFHMCLauncher  ·  FLFHMCL 2.1.6  ·  FLF API 1.2", F["small"], SUB,
              anchor="ma", alpha=int(180 * p))
    draw_text(layer, cx, 900, "Hello Minecraft! Launcher  ×  Fabric Loader", F["small"], SUB,
              anchor="ma", alpha=int(150 * p))

def scene_transition(layer, t):
    # 简单黑场闪切
    pts = [6, 15, 25, 36, 47.5, 58.5, 69.5, 79]
    a = 0
    for p in pts:
        d = abs(t - p)
        if d < 0.22:
            a = max(a, int(160 * (1 - d / 0.22)))
    if a:
        layer.alpha_composite(Image.new("RGBA", (W, H), (2, 5, 10, a)))

def render(i):
    t = i / FPS
    img = base_scene(t)
    # 场景
    scene_intro(img, t)
    scene_problem(img, t)
    scene_architecture(img, t)
    scene_launcher(img, t)
    scene_install(img, t)
    scene_matrix(img, t)
    scene_api(img, t)
    scene_crash(img, t)
    scene_end(img, t)
    scene_transition(img, t)
    vignette(img)
    # 底部进度线
    d = ImageDraw.Draw(img)
    d.rectangle((0, H - 5, W, H), fill=(5, 9, 18, 255))
    p = t / DURATION
    d.rectangle((0, H - 5, int(W * p), H), fill=(59, 130, 246, 220))
    img.convert("RGB").save(OUT / f"{i:05d}.jpg", quality=94)
    return i

def render_all():
    from multiprocessing import Pool
    total = int(DURATION * FPS)
    with Pool(processes=max(2, os.cpu_count() // 2)) as pool:
        for n, _ in enumerate(pool.imap_unordered(render, range(total), chunksize=8), 1):
            if n % 120 == 0:
                print(f"rendered {n}/{total}")

def make_audio(path):
    sr = 44100
    total = int(DURATION * sr)
    frames = bytearray()
    # 简单环境铺底：Dm - Bb - F - C
    chords = [(146.83, 174.61, 220.00), (116.54, 146.83, 174.61),
              (174.61, 220.00, 261.63), (130.81, 164.81, 196.00)]
    for n in range(total):
        t = n / sr
        chord = chords[int(t / 8) % 4]
        val = 0.0
        for k, f in enumerate(chord):
            val += math.sin(2 * math.pi * f * t + k) * 0.035
        # 缓慢脉冲
        beat = 0.5 + 0.5 * math.sin(2 * math.pi * 0.1 * t)
        val *= (0.55 + 0.45 * beat)
        # 场景切换 whoosh
        for p in (6, 15, 25, 36, 47.5, 58.5, 69.5, 79):
            dt = t - p
            if 0 <= dt < 0.35:
                val += math.sin(2 * math.pi * (120 - dt * 200) * dt) * 0.06 * (1 - dt / 0.35)
        s = int(clamp(val, -1, 1) * 32767)
        frames += s.to_bytes(2, "little", signed=True) * 2
    with wave.open(str(path), "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(sr)
        w.writeframes(bytes(frames))

def main():
    import sys
    if "--audio" in sys.argv:
        make_audio(ROOT / "audio.wav")
        return
    render_all()
    subprocess.run([
        r"D:\Program Files\ffmpeg\bin\ffmpeg.exe", "-y",
        "-framerate", str(FPS), "-i", str(OUT / "%05d.jpg"),
        "-i", str(ROOT / "audio.wav") if (ROOT / "audio.wav").exists() else "anullsrc",
        "-c:v", "libx264", "-preset", "medium", "-crf", "18",
        "-pix_fmt", "yuv420p", "-c:a", "aac", "-b:a", "160k",
        "-shortest", str(ROOT / "FLFHMCL-介绍.mp4"),
    ], check=False)

if __name__ == "__main__":
    main()
