# -*- coding: utf-8 -*-
"""FLFHMCL 亮色宣传片：90 秒，跟随 Dragonflame 节拍。"""
import math
import os
import random
import subprocess
import wave
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

W, H = 1920, 1080
FPS = 30
DURATION = 90.0
BPM = 107.67
BEAT = 60.0 / BPM
ROOT = Path(__file__).resolve().parent
OUT = ROOT / "promo_frames"
OUT.mkdir(parents=True, exist_ok=True)

FONT_CN = r"C:\Windows\Fonts\msyh.ttc"
FONT_CN_B = r"C:\Windows\Fonts\msyhbd.ttc"
FONT_EN = r"C:\Windows\Fonts\segoeuib.ttf"
FONT_MONO = r"C:\Windows\Fonts\consola.ttf"

def font(path, size):
    return ImageFont.truetype(path, size)

F = {
    "hero": font(FONT_CN_B, 116),
    "h1": font(FONT_CN_B, 84),
    "h2": font(FONT_CN_B, 56),
    "h3": font(FONT_CN_B, 36),
    "body": font(FONT_CN, 30),
    "small": font(FONT_CN, 24),
    "tiny": font(FONT_CN, 19),
    "mono": font(FONT_MONO, 24),
    "mono_s": font(FONT_MONO, 18),
    "num": font(FONT_EN, 76),
}

SCREEN_IMG = Image.open(r"K:\FLFHMCLauncher\build\_dark3.png").convert("RGB")
ICON_IMG = Image.open(r"K:\FLFHMCLauncher\assets\icon\flfhmlauncher-256.png").convert("RGBA")

TEXT = (15, 23, 42)
SUB = (71, 85, 105)
MUTED = (100, 116, 139)
BLUE = (37, 99, 235)
SKY = (14, 165, 233)
PURPLE = (124, 58, 237)
CYAN = (8, 145, 178)
GREEN = (5, 150, 105)
ORANGE = (217, 119, 6)
WHITE = (255, 255, 255)

def clamp(v, a=0.0, b=1.0):
    return max(a, min(b, v))

def smooth(x):
    x = clamp(x)
    return x * x * (3 - 2 * x)

def ease_out(x):
    x = clamp(x)
    return 1 - (1 - x) ** 3

def ease_back(x):
    x = clamp(x)
    c = 1.70158
    return 1 + (c + 1) * (x - 1) ** 3 + c * (x - 1) ** 2

def fade_window(t, a, b, fade=0.55):
    if t < a or t > b:
        return 0.0
    return smooth((t - a) / fade) * smooth((b - t) / fade)

def lerp(a, b, p):
    return a + (b - a) * p

def beat_phase(t):
    return ((t / BEAT) % 1.0)

def pulse(t, sharp=5.0):
    return math.exp(-beat_phase(t) * sharp)

def text_w(s, f):
    return ImageDraw.Draw(Image.new("RGB", (8, 8))).textlength(s, font=f)

def draw_text(layer, x, y, s, f, fill, anchor="la", alpha=255):
    if alpha <= 2:
        return
    d = ImageDraw.Draw(layer)
    color = fill if len(fill) == 4 else (*fill, alpha)
    d.text((x, y), s, font=f, fill=color, anchor=anchor)

def light_bg():
    img = Image.new("RGB", (W, H), (248, 250, 255))
    px = img.load()
    top = (250, 252, 255)
    bottom = (226, 237, 252)
    for y in range(H):
        p = y / H
        c = (int(lerp(top[0], bottom[0], p)), int(lerp(top[1], bottom[1], p)),
             int(lerp(top[2], bottom[2], p)))
        for x in range(W):
            px[x, y] = c
    return img

BG = light_bg()

def radial_glow(radius, color, alpha=110):
    size = radius * 2
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    for r in range(radius, 0, -4):
        p = 1 - r / radius
        a = int(alpha * (p ** 2.1))
        d.ellipse((radius - r, radius - r, radius + r, radius + r),
                  fill=(*color, a))
    return img.filter(ImageFilter.GaussianBlur(20))

GLOW_BLUE = radial_glow(460, BLUE, 100)
GLOW_PURPLE = radial_glow(420, PURPLE, 80)
GLOW_CYAN = radial_glow(380, SKY, 70)

def paste_glow(base, glow, cx, cy, scale=1.0, alpha=1.0):
    if scale <= 0.01 or alpha <= 0.01:
        return
    g = glow
    if abs(scale - 1) > 0.01:
        g = glow.resize((int(glow.width * scale), int(glow.height * scale)))
    if alpha < 1:
        g = g.copy()
        g.putalpha(g.getchannel("A").point(lambda v: int(v * alpha)))
    base.alpha_composite(g, (int(cx - g.width / 2), int(cy - g.height / 2)))

def base_scene(t):
    img = BG.convert("RGBA")
    p = pulse(t)
    paste_glow(img, GLOW_BLUE, 1500 + 60 * math.sin(t * 0.22), 180, 1.05 + 0.04 * p, 0.55)
    paste_glow(img, GLOW_PURPLE, 260 + 50 * math.cos(t * 0.2), 840, 0.9, 0.38)
    paste_glow(img, GLOW_CYAN, 900 + 80 * math.sin(t * 0.12), 920, 0.7, 0.25)
    d = ImageDraw.Draw(img)
    # 点阵
    for y in range(90, H, 64):
        for x in range(80, W, 64):
            a = 24 + int(10 * math.sin(t * 1.5 + x * 0.01 + y * 0.02))
            d.ellipse((x, y, x + 2, y + 2), fill=(59, 130, 246, a))
    # 漂浮柔光圆
    for i in range(5):
        r = 8 + (i * 7) % 20
        x = (200 + i * 380 + t * (10 + i * 3)) % (W + 100) - 50
        y = 140 + (i * 190) % 700 + 20 * math.sin(t * 0.4 + i)
        d.ellipse((x - r, y - r, x + r, y + r),
                  fill=(37, 99, 235, 16 + i * 4))
    return img

def shadow_rect(layer, box, radius=24, blur=24, alpha=55, offset=(0, 12)):
    w = box[2] - box[0] + 80
    h = box[3] - box[1] + 80
    sh = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(sh)
    d.rounded_rectangle((40, 40, 40 + box[2] - box[0], 40 + box[3] - box[1]),
                        radius=radius, fill=(15, 35, 70, alpha))
    sh = sh.filter(ImageFilter.GaussianBlur(blur))
    layer.alpha_composite(sh, (int(box[0] - 40 + offset[0]), int(box[1] - 40 + offset[1])))

def card(layer, box, radius=24, fill=(255, 255, 255, 242),
         outline=(191, 219, 254, 255), width=2, shadow=True):
    if shadow:
        shadow_rect(layer, box, radius=radius)
    ImageDraw.Draw(layer).rounded_rectangle(box, radius=radius, fill=fill,
                                           outline=outline, width=width)

def pill(layer, x, y, text, f, fill, fg=WHITE, pad_x=18, pad_y=9):
    d = ImageDraw.Draw(layer)
    tw = text_w(text, f)
    box = (x, y, x + tw + pad_x * 2, y + f.size + pad_y * 2)
    d.rounded_rectangle(box, radius=(box[3] - box[1]) // 2, fill=fill)
    d.text((x + pad_x, y + pad_y - 2), text, font=f, fill=fg)
    return box

def reveal_text(layer, x, y, s, f, fill, p, anchor="la"):
    p = clamp(p)
    if p <= 0:
        return
    w = max(4, int(text_w(s, f) * p) + 4)
    tmp = Image.new("RGBA", (w, f.size + 30), (0, 0, 0, 0))
    ImageDraw.Draw(tmp).text((0, 0), s, font=f, fill=(*fill, 255))
    layer.alpha_composite(tmp, (int(x), int(y - 10)))

def beat_flash(layer, t):
    p = beat_phase(t)
    a = int(26 * math.exp(-p * 9))
    if a > 0:
        layer.alpha_composite(Image.new("RGBA", (W, H), (255, 255, 255, a)))

def scene_intro(layer, t):
    p = fade_window(t, 0, 8.0, 0.5)
    if p <= 0:
        return
    b = pulse(t, 4)
    cx, cy = W // 2, 390
    # 柔和圆环
    for i in range(3):
        r = int(150 + i * 52 + 10 * b)
        a = int(100 * (1 - i / 3) * p)
        ImageDraw.Draw(layer).ellipse((cx - r, cy - r, cx + r, cy + r),
                                      outline=(37, 99, 235, a), width=2)
    logo_p = ease_back(clamp(t / 0.85))
    size = int(220 * (0.65 + 0.35 * logo_p))
    layer.alpha_composite(ICON_IMG.resize((size, size)), (cx - size // 2, cy - size // 2))
    paste_glow(layer, GLOW_BLUE, cx, cy + 10, 0.55 + 0.05 * b, 0.45 * p)

    title = "FLFHMCL"
    f = F["hero"]
    tw = text_w(title, f)
    reveal_text(layer, cx - tw / 2, cy + 165, title, f, TEXT, smooth((t - 0.6) / 0.65))
    draw_text(layer, cx, cy + 300, "让 HMCL 跨版本运行", F["h2"], BLUE,
              anchor="ma", alpha=int(255 * smooth((t - 1.4) / 0.7)))
    draw_text(layer, cx, cy + 370, "一个启动器，一个窗口，一套组件", F["body"], SUB,
              anchor="ma", alpha=int(230 * smooth((t - 2.0) / 0.7)))
    # 三个标签依次弹入
    labels = [("跨版本", BLUE), ("自包含", PURPLE), ("可扩展", CYAN)]
    total = 0
    for _, _ in labels:
        total += 170
    start = cx - total // 2
    for i, (text, color) in enumerate(labels):
        app = ease_back(clamp((t - 2.9 - i * 0.16) / 0.45))
        if app > 0:
            x = start + i * 170
            pill(layer, x, cy + 445, text, F["small"],
                 (*color, 25), fg=color, pad_x=24, pad_y=8)

def scene_problem(layer, t):
    p = fade_window(t, 7.7, 16.4, 0.5)
    if p <= 0:
        return
    draw_text(layer, 130, 120, "但用 HMCL，总是这些麻烦", F["h1"], TEXT, alpha=int(255 * p))
    draw_text(layer, 134, 225, "版本越多，维护成本越高", F["body"], SUB, alpha=int(220 * p))

    items = [
        ("版本太多", "每个 HMCL 版本都要重新适配", BLUE),
        ("安装太碎", "Java、Fabric、依赖、资源，一步不能少", PURPLE),
        ("崩溃难查", "日志一长串，不知道问题在哪", ORANGE),
    ]
    for i, (title, sub, color) in enumerate(items):
        app = ease_out(clamp((t - 8.2 - i * 0.25) / 0.6))
        if app <= 0:
            continue
        x = 180 + i * 535
        y = 380 - int((1 - app) * 50)
        box = (x, y, x + 490, y + 250)
        card(layer, box)
        d = ImageDraw.Draw(layer)
        d.ellipse((x + 30, y + 28, x + 86, y + 84), fill=(*color, 32))
        d.text((x + 46, y + 38), "!", font=F["h3"], fill=color)
        draw_text(layer, x + 30, y + 108, title, F["h3"], TEXT, alpha=int(255 * app))
        draw_text(layer, x + 30, y + 160, sub, F["small"], SUB, alpha=int(220 * app))
        # 进度线装饰
        d.rounded_rectangle((x + 30, y + 205, x + 450, y + 211), radius=3, fill=(226, 232, 240))
        d.rounded_rectangle((x + 30, y + 205, x + 30 + int(120 * app), y + 211), radius=3, fill=(*color, 170))

    draw_text(layer, W // 2, 760, "FLFHMCLauncher 想把这些事，一次做完", F["h2"], BLUE,
              anchor="ma", alpha=int(240 * smooth((t - 11.2) / 0.7)))

def screenshot(layer, x, y, w, h, p):
    if p <= 0:
        return
    scale = 0.9 + 0.1 * ease_out(p)
    ww, hh = int(w * scale), int(h * scale)
    shot = SCREEN_IMG.resize((ww, hh))
    mask = Image.new("L", (ww, hh), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, ww, hh), radius=20, fill=255)
    shadow_rect(layer, (x, y, x + ww, y + hh), radius=20, blur=30, alpha=55, offset=(0, 18))
    layer.paste(shot, (int(x), int(y)), mask)
    ImageDraw.Draw(layer).rounded_rectangle((x, y, x + ww, y + hh), radius=20,
                                            outline=(147, 197, 253, 255), width=2)

def scene_launcher(layer, t):
    p = fade_window(t, 15.9, 27.6, 0.55)
    if p <= 0:
        return
    draw_text(layer, 120, 92, "FLFHMCLauncher", F["h2"], BLUE, alpha=int(230 * p))
    draw_text(layer, 124, 160, "一个窗口，管好所有实例", F["h1"], TEXT, alpha=int(255 * p))
    draw_text(layer, 128, 268, "新建、安装、启动、更新、修复，全都在这里", F["body"], SUB,
              alpha=int(220 * p))

    app = ease_out(clamp((t - 16.6) / 0.8))
    x = int(lerp(1250, 140, app))
    screenshot(layer, x, 330, 1080, 748, p)

    feats = [
        ("多实例并行", "同时开多个版本，互不影响", BLUE),
        ("模组与目录", "独立隔离，一键打开", PURPLE),
        ("版本与更新", "HMCL / FLFHMCL / Fabric 都能选", CYAN),
        ("快速启动器", "拖到桌面，双击即玩", GREEN),
    ]
    for i, (title, sub, color) in enumerate(feats):
        app2 = smooth((t - 18.2 - i * 0.28) / 0.5)
        if app2 <= 0:
            continue
        x2 = 1280
        y2 = 320 + i * 130
        d = ImageDraw.Draw(layer)
        d.rounded_rectangle((x2, y2, x2 + 500, y2 + 100), radius=18,
                            fill=(255, 255, 255, int(230 * app2)),
                            outline=(*color, int(80 * app2)), width=2)
        d.ellipse((x2 + 24, y2 + 28, x2 + 68, y2 + 72), fill=(*color, int(28 * app2)))
        d.ellipse((x2 + 38, y2 + 42, x2 + 54, y2 + 58), fill=(*color, int(200 * app2)))
        draw_text(layer, x2 + 88, y2 + 20, title, F["small"], TEXT, alpha=int(255 * app2))
        draw_text(layer, x2 + 88, y2 + 55, sub, F["tiny"], SUB, alpha=int(210 * app2))

def scene_install(layer, t):
    p = fade_window(t, 27.2, 38.4, 0.55)
    if p <= 0:
        return
    draw_text(layer, 120, 90, "一键安装", F["h1"], TEXT, alpha=int(255 * p))
    draw_text(layer, 124, 200, "下载、校验、安装，全程看得见", F["body"], SUB, alpha=int(220 * p))

    box = (160, 285, 1760, 930)
    card(layer, box, radius=28, fill=(255, 255, 255, 248))
    d = ImageDraw.Draw(layer)
    # 左侧总进度
    d.rounded_rectangle((205, 330, 650, 880), radius=22, fill=(239, 246, 255, 255),
                        outline=(191, 219, 254, 255), width=1)
    draw_text(layer, 240, 365, "安装进度", F["small"], SUB)
    prog = ease_out(clamp((t - 28.0) / 7.8))
    pct = int(prog * 100)
    d.text((240, 405), f"{pct}%", font=F["num"], fill=BLUE)
    d.rounded_rectangle((240, 520, 610, 540), radius=10, fill=(219, 234, 254))
    d.rounded_rectangle((240, 520, 240 + int(370 * prog), 540), radius=10, fill=BLUE)
    for i, line in enumerate(["准备 Java", "安装 Fabric", "下载 HMCL", "写入配置"]):
        yy = 600 + i * 55
        done = pct > (i + 1) * 22
        d.ellipse((240, yy + 6, 258, yy + 24), fill=GREEN if done else (203, 213, 225))
        draw_text(layer, 274, yy, line, F["small"], TEXT if done else SUB, alpha=235)

    steps = ["准备 Java 运行时", "安装 Fabric Loader", "下载 HMCL 与依赖", "写入实例配置"]
    for i, name in enumerate(steps):
        yy = 340 + i * 128
        done = prog * 4 > i + 1
        active = prog * 4 > i and not done
        d.rounded_rectangle((700, yy, 1715, yy + 104), radius=16,
                            fill=(248, 250, 252, 255) if not done else (240, 253, 244, 255),
                            outline=(187, 247, 208, 255) if done else ((147, 197, 253, 255) if active else (226, 232, 240, 255)),
                            width=2)
        d.ellipse((728, yy + 34, 764, yy + 70), fill=GREEN if done else (BLUE if active else (203, 213, 225)))
        d.text((740, yy + 40), "OK" if done else ("..." if active else "-"), font=F["tiny"],
               fill=(255, 255, 255) if (done or active) else (100, 116, 139))
        draw_text(layer, 788, yy + 22, name, F["small"], TEXT, alpha=240)
        d.text((1590, yy + 28), "100%" if done else (f"{int(prog * 100) if active else 0}%"),
               font=F["small"], fill=GREEN if done else SUB)
        local = 1 if done else (clamp(prog * 4 - i) if active else 0)
        d.rounded_rectangle((788, yy + 70, 1640, yy + 78), radius=4, fill=(226, 232, 240))
        if local > 0:
            d.rounded_rectangle((788, yy + 70, 788 + int(852 * local), yy + 78), radius=4,
                                fill=GREEN if done else BLUE)

def scene_cross(layer, t):
    p = fade_window(t, 37.8, 48.9, 0.55)
    if p <= 0:
        return
    draw_text(layer, W // 2, 110, "一套 FLFHMCL，多个 HMCL", F["h1"], TEXT,
              anchor="ma", alpha=int(255 * p))
    draw_text(layer, W // 2, 220, "不用为每个版本准备一份组件", F["body"], SUB,
              anchor="ma", alpha=int(220 * p))

    versions = ["3.14", "3.15", "3.16", "3.17"]
    for i, v in enumerate(versions):
        app = ease_back(clamp((t - 38.4 - i * 0.18) / 0.5))
        if app <= 0:
            continue
        x = 230 + i * 390
        y = 400 - int((1 - app) * 70)
        d = ImageDraw.Draw(layer)
        d.rounded_rectangle((x, y, x + 330, y + 210), radius=26,
                            fill=(255, 255, 255, int(245 * app)),
                            outline=(*BLUE, int(80 * app)), width=2)
        d.text((x + 36, y + 28), "HMCL", font=F["small"], fill=(*MUTED, int(255 * app)))
        d.text((x + 36, y + 74), v, font=F["num"], fill=(*TEXT, int(255 * app)))
        d.rounded_rectangle((x + 36, y + 165, x + 120, y + 172), radius=4, fill=(*GREEN, int(200 * app)))
        draw_text(layer, x + 132, y + 155, "已适配", F["tiny"], GREEN, alpha=int(255 * app))

    app = smooth((t - 40.5) / 0.6)
    if app > 0:
        d = ImageDraw.Draw(layer)
        bx, by = W // 2 - 360, 760
        d.rounded_rectangle((bx, by, bx + 720, by + 110), radius=55,
                            fill=(*BLUE, int(245 * app)))
        d.text((bx + 42, by + 24), "FLFHMCL 2.1.6", font=F["h3"], fill=(255, 255, 255, int(255 * app)))
        d.text((bx + 420, by + 30), "Knot + Mixin", font=F["small"], fill=(224, 242, 254, int(255 * app)))

def field_row(layer, x, y, label, value, color, app):
    d = ImageDraw.Draw(layer)
    d.rounded_rectangle((x, y, x + 760, y + 78), radius=14,
                        fill=(248, 250, 252, int(240 * app)),
                        outline=(226, 232, 240, int(255 * app)), width=1)
    draw_text(layer, x + 24, y + 22, label, F["small"], SUB, alpha=int(230 * app))
    d.rounded_rectangle((x + 300, y + 18, x + 735, y + 60), radius=10,
                        fill=(255, 255, 255, int(255 * app)),
                        outline=(*color, int(120 * app)), width=1)
    draw_text(layer, x + 320, y + 26, value, F["tiny"], TEXT, alpha=int(255 * app))
    d.text((x + 705, y + 26), "v", font=F["tiny"], fill=(*MUTED, int(255 * app)))

def scene_update(layer, t):
    p = fade_window(t, 48.4, 58.8, 0.55)
    if p <= 0:
        return
    draw_text(layer, 120, 90, "换版本，不用重装", F["h1"], TEXT, alpha=int(255 * p))
    draw_text(layer, 124, 200, "HMCL、FLFHMCL、Fabric 都可以在这里切换", F["body"], SUB,
              alpha=int(220 * p))

    box = (480, 290, 1440, 820)
    card(layer, box, radius=28, fill=(255, 255, 255, 250))
    draw_text(layer, 520, 320, "更新实例组件", F["h3"], TEXT, alpha=int(255 * p))
    rows = [("HMCL", "3.16.4", BLUE), ("FLFHMCL", "2.1.6", PURPLE), ("Fabric Loader", "0.20.1", CYAN)]
    for i, (name, value, color) in enumerate(rows):
        app = smooth((t - 49.2 - i * 0.16) / 0.45)
        if app > 0:
            field_row(layer, 520, 385 + i * 95, name, value, color, app)

    app = smooth((t - 51.0) / 0.5)
    if app > 0:
        d = ImageDraw.Draw(layer)
        d.rounded_rectangle((520, 685, 840, 740), radius=27, fill=(*BLUE, int(240 * app)))
        d.text((568, 700), "开始更新", font=F["small"], fill=(255, 255, 255, int(255 * app)))
        draw_text(layer, 870, 700, "自动补齐缺失文件", F["tiny"], SUB, alpha=int(230 * app))

    tips = ["选择版本", "自动补全", "失败修复"]
    for i, tip in enumerate(tips):
        app2 = smooth((t - 53.0 - i * 0.25) / 0.5)
        if app2 <= 0:
            continue
        x = 1500
        y = 360 + i * 110
        d = ImageDraw.Draw(layer)
        d.ellipse((x, y, x + 16, y + 16), fill=(*GREEN, int(220 * app2)))
        draw_text(layer, x + 34, y - 6, tip, F["small"], TEXT, alpha=int(240 * app2))

def jar_icon(layer, x, y, color, label, app):
    d = ImageDraw.Draw(layer)
    d.rounded_rectangle((x, y, x + 104, y + 128), radius=12,
                        fill=(255, 255, 255, int(245 * app)),
                        outline=(*color, int(150 * app)), width=2)
    d.rectangle((x + 22, y + 28, x + 82, y + 100), outline=(*color, int(180 * app)), width=2)
    d.text((x + 34, y + 45), "JAR", font=F["tiny"], fill=(*color, int(255 * app)))
    draw_text(layer, x + 52, y + 138, label, F["tiny"], SUB, anchor="ma", alpha=int(220 * app))

def scene_mclhr(layer, t):
    p = fade_window(t, 58.2, 69.6, 0.55)
    if p <= 0:
        return
    draw_text(layer, 120, 90, "mclhr", F["h1"], PURPLE, alpha=int(255 * p))
    draw_text(layer, 124, 200, "一份库，多个实例共用", F["h2"], TEXT, alpha=int(255 * p))
    draw_text(layer, 128, 280, "自动解析、共享缓存，少下载，少占空间", F["body"], SUB,
              alpha=int(220 * p))

    # 中央库堆
    stack = [(760, 340, BLUE, "JavaFX"), (760, 510, PURPLE, "Fabric"), (760, 680, CYAN, "依赖库")]
    for i, (x, y, color, label) in enumerate(stack):
        app = ease_out(clamp((t - 59.0 - i * 0.15) / 0.45))
        if app <= 0:
            continue
        jar_icon(layer, x, y, color, label, app)

    # 实例节点
    inst = [(1300, 390, "实例 A"), (1340, 560, "实例 B"), (1280, 720, "实例 C")]
    for i, (x, y, name) in enumerate(inst):
        app = smooth((t - 60.0 - i * 0.2) / 0.5)
        if app <= 0:
            continue
        d = ImageDraw.Draw(layer)
        d.rounded_rectangle((x, y, x + 260, y + 90), radius=18,
                            fill=(255, 255, 255, int(240 * app)),
                            outline=(*PURPLE, int(80 * app)), width=2)
        d.ellipse((x + 24, y + 30, x + 54, y + 60), fill=(*GREEN, int(220 * app)))
        draw_text(layer, x + 72, y + 26, name, F["small"], TEXT, alpha=int(255 * app))
        # 连线
        sx, sy = 864, 404 + i * 170
        d.line((sx, sy, x, y + 45), fill=(*PURPLE, int(90 * app)), width=2)
        # 流光
        q = (t * 0.8 + i * 0.3) % 1
        px = lerp(sx, x, q)
        py = lerp(sy, y + 45, q)
        d.ellipse((px - 5, py - 5, px + 5, py + 5), fill=(*PURPLE, int(255 * app)))

    # 底部说明
    app = smooth((t - 61.5) / 0.5)
    if app > 0:
        d = ImageDraw.Draw(layer)
        d.rounded_rectangle((760, 850, 1240, 910), radius=30,
                            fill=(*PURPLE, int(24 * app)))
        d.text((800, 865), "自动解析  ·  共享缓存  ·  跨实例复用", font=F["small"],
               fill=(*PURPLE, int(255 * app)))

def scene_crash(layer, t):
    p = fade_window(t, 69.2, 79.2, 0.55)
    if p <= 0:
        return
    draw_text(layer, 120, 90, "崩溃了，也看得懂", F["h1"], TEXT, alpha=int(255 * p))
    draw_text(layer, 124, 200, "报告自动保存，问题直接说人话", F["body"], SUB, alpha=int(220 * p))

    box = (170, 300, 1040, 800)
    card(layer, box, radius=24, fill=(255, 255, 255, 250))
    d = ImageDraw.Draw(layer)
    d.rounded_rectangle((170, 300, 1040, 360), radius=24, fill=(254, 242, 242, 255))
    d.ellipse((206, 320, 230, 344), fill=(239, 68, 68))
    d.ellipse((244, 320, 268, 344), fill=(251, 191, 36))
    d.ellipse((282, 320, 306, 344), fill=(34, 197, 94))
    draw_text(layer, 206, 390, "HMCL 异常退出", F["h3"], TEXT, alpha=int(255 * p))
    draw_text(layer, 206, 445, "检测到重复的 Fabric Loader 版本", F["small"], ORANGE, alpha=int(240 * p))
    draw_text(layer, 206, 495, "退出代码：1", F["small"], SUB, alpha=int(230 * p))
    d.rounded_rectangle((206, 545, 1000, 555), radius=4, fill=(226, 232, 240))
    for i, line in enumerate(["建议更新到 FLFHMCL 2.1.6", "或点击下方按钮自动修复实例"]):
        draw_text(layer, 206, 585 + i * 44, line, F["small"], TEXT, alpha=int(220 * p))
    d.rounded_rectangle((206, 700, 480, 760), radius=28, fill=(*BLUE, int(240 * p)))
    d.text((246, 716), "修复实例", font=F["small"], fill=(255, 255, 255, int(255 * p)))
    d.rounded_rectangle((510, 700, 690, 760), radius=28, fill=(241, 245, 249, int(255 * p)))
    d.text((550, 716), "查看报告", font=F["small"], fill=(*TEXT, int(255 * p)))

    # 右侧文件卡
    app = smooth((t - 71.0) / 0.6)
    if app > 0:
        x, y = 1180, 350
        d = ImageDraw.Draw(layer)
        d.rounded_rectangle((x, y, x + 560, y + 330), radius=22,
                            fill=(255, 255, 255, int(245 * app)),
                            outline=(*GREEN, int(130 * app)), width=2)
        d.text((x + 34, y + 28), "crash-report", font=F["h3"], fill=(*TEXT, int(255 * app)))
        for i, line in enumerate([".hmcl/crash-report/", "flfhmcl-crash-2026...txt",
                                  "已保存  ·  已复制诊断"]):
            d.text((x + 34, y + 105 + i * 56), line,
                   font=(F["tiny"] if i == 2 else F["mono_s"]),
                   fill=((SUB if i < 2 else GREEN) + (int(240 * app),)))
        d.rounded_rectangle((x + 34, y + 260, x + 300, y + 300), radius=20,
                            fill=(*GREEN, int(30 * app)))
        d.text((x + 56, y + 266), "一键反馈", font=F["small"], fill=(*GREEN, int(255 * app)))

def scene_end(layer, t):
    p = fade_window(t, 78.8, 90.3, 0.7)
    if p <= 0:
        return
    b = pulse(t, 4)
    cx, cy = W // 2, 360
    size = 210
    layer.alpha_composite(ICON_IMG.resize((size, size)), (cx - size // 2, cy - size // 2))
    paste_glow(layer, GLOW_BLUE, cx, cy, 0.65 + 0.05 * b, 0.35 * p)
    draw_text(layer, cx, cy + 150, "FLFHMCL", F["hero"], TEXT, anchor="ma", alpha=int(255 * p))
    draw_text(layer, cx, cy + 300, "下载即用，跨版本运行", F["h2"], BLUE,
              anchor="ma", alpha=int(245 * smooth((t - 80.0) / 0.6)))
    draw_text(layer, cx, cy + 380, "FLFHMCLauncher  ·  mclhr  ·  FLF API", F["body"], SUB,
              anchor="ma", alpha=int(200 * p))
    labels = [("3.14", BLUE), ("3.15", PURPLE), ("3.16", CYAN), ("3.17", ORANGE)]
    for i, (text, color) in enumerate(labels):
        x = cx - 310 + i * 170
        y = 850
        d = ImageDraw.Draw(layer)
        d.rounded_rectangle((x, y, x + 140, y + 58), radius=29,
                            fill=(*color, int(22 * p)), outline=(*color, int(80 * p)), width=1)
        d.text((x + 46, y + 14), text, font=F["small"], fill=(*color, int(255 * p)))

def transition(layer, t):
    cuts = [7.8, 16.0, 27.0, 37.9, 48.4, 58.1, 69.1, 78.7]
    a = 0
    for c in cuts:
        d = abs(t - c)
        if d < 0.16:
            a = max(a, int(110 * (1 - d / 0.16)))
    if a:
        layer.alpha_composite(Image.new("RGBA", (W, H), (255, 255, 255, a)))

def render(i):
    t = i / FPS
    img = base_scene(t)
    scene_intro(img, t)
    scene_problem(img, t)
    scene_launcher(img, t)
    scene_install(img, t)
    scene_cross(img, t)
    scene_update(img, t)
    scene_mclhr(img, t)
    scene_crash(img, t)
    scene_end(img, t)
    beat_flash(img, t)
    transition(img, t)
    d = ImageDraw.Draw(img)
    d.rectangle((0, H - 4, W, H), fill=(255, 255, 255, 255))
    d.rectangle((0, H - 4, int(W * t / DURATION), H), fill=(*BLUE, 220))
    img = Image.alpha_composite(Image.new("RGBA", (W, H), (255, 255, 255, 255)), img)
    img.convert("RGB").save(OUT / f"{i:05d}.jpg", quality=94)
    return i

def render_all():
    from multiprocessing import Pool
    total = int(DURATION * FPS)
    with Pool(processes=max(2, os.cpu_count() // 2)) as pool:
        for n, _ in enumerate(pool.imap_unordered(render, range(total), chunksize=8), 1):
            if n % 150 == 0:
                print(f"rendered {n}/{total}")

def main():
    render_all()
    # 15s 起截取 90s，淡入淡出
    subprocess.run([
        r"D:\Program Files\ffmpeg\bin\ffmpeg.exe", "-y",
        "-framerate", str(FPS), "-i", str(OUT / "%05d.jpg"),
        "-ss", "15", "-t", "90", "-i", r"K:\Music\Dragonflame - Kirara Magic.flac",
        "-filter_complex", "[1:a]afade=t=in:st=0:d=1.2,afade=t=out:st=87:d=3[a]",
        "-map", "0:v:0", "-map", "[a]",
        "-c:v", "libx264", "-preset", "medium", "-crf", "18",
        "-pix_fmt", "yuv420p", "-c:a", "aac", "-b:a", "192k",
        "-shortest", str(ROOT / "FLFHMCL-宣传片.mp4"),
    ], check=False)

if __name__ == "__main__":
    main()
