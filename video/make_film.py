# -*- coding: utf-8 -*-
"""FLFHMCL 2:30 亮色宣传片：FLFHMCL 为主，FLFHMCLauncher 为可选的快速启动入口。"""
import math
import os
import random
import subprocess
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

W, H = 1920, 1080
FPS = 30
DURATION = 150.0
BPM = 107.67
BEAT = 60.0 / BPM
ROOT = Path(__file__).resolve().parent
OUT = ROOT / "film_frames"
OUT.mkdir(parents=True, exist_ok=True)

CN = r"C:\Windows\Fonts\msyh.ttc"
CNB = r"C:\Windows\Fonts\msyhbd.ttc"
ENB = r"C:\Windows\Fonts\segoeuib.ttf"
MONO = r"C:\Windows\Fonts\consola.ttf"

def ft(p, s):
    return ImageFont.truetype(p, s)

F = {
    "hero": ft(CNB, 122),
    "h1": ft(CNB, 88),
    "h2": ft(CNB, 60),
    "h3": ft(CNB, 38),
    "body": ft(CN, 31),
    "small": ft(CN, 24),
    "tiny": ft(CN, 19),
    "mono": ft(MONO, 22),
    "num": ft(ENB, 80),
}

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

HMCL_SCREEN = Image.open(r"K:\FLFHMCLauncher\video\hmcl.png").convert("RGB")
LAUNCHER_SCREEN = Image.open(r"K:\FLFHMCLauncher\build\_dark3.png").convert("RGB")
ICON = Image.open(r"K:\FLFHMCLauncher\assets\icon\flfhmlauncher-256.png").convert("RGBA")

def clamp(v, a=0.0, b=1.0):
    return max(a, min(b, v))

def smooth(x):
    x = clamp(x)
    return x * x * (3 - 2 * x)

def ease(x):
    x = clamp(x)
    return 1 - (1 - x) ** 3

def back(x):
    x = clamp(x)
    c = 1.70158
    return 1 + (c + 1) * (x - 1) ** 3 + c * (x - 1) ** 2

def lerp(a, b, p):
    return a + (b - a) * p

def fade(t, a, b, f=0.55):
    if t < a or t > b:
        return 0.0
    return smooth((t - a) / f) * smooth((b - t) / f)

def pulse(t, sharp=5.0):
    return math.exp(-((t / BEAT) % 1.0) * sharp)

def tw(s, f):
    return ImageDraw.Draw(Image.new("RGB", (8, 8))).textlength(s, font=f)

def text(layer, x, y, s, f, fill, anchor="la", alpha=255):
    if alpha <= 2:
        return
    ImageDraw.Draw(layer).text((x, y), s, font=f,
                               fill=(fill if len(fill) == 4 else (*fill, alpha)),
                               anchor=anchor)

def bg_base():
    img = Image.new("RGB", (W, H), (249, 251, 255))
    px = img.load()
    top, bot = (250, 252, 255), (226, 237, 252)
    for y in range(H):
        p = y / H
        c = (int(lerp(top[0], bot[0], p)), int(lerp(top[1], bot[1], p)),
             int(lerp(top[2], bot[2], p)))
        for x in range(W):
            px[x, y] = c
    return img

BG = bg_base()

def glow(radius, color, alpha):
    img = Image.new("RGBA", (radius * 2, radius * 2), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    for r in range(radius, 0, -4):
        p = 1 - r / radius
        d.ellipse((radius - r, radius - r, radius + r, radius + r),
                  fill=(*color, int(alpha * p ** 2.1)))
    return img.filter(ImageFilter.GaussianBlur(20))

G_BLUE = glow(470, BLUE, 100)
G_PURPLE = glow(430, PURPLE, 80)
G_CYAN = glow(380, SKY, 70)

def paste_glow(base, g, cx, cy, scale=1.0, alpha=1.0):
    if alpha <= 0.01:
        return
    if abs(scale - 1) > 0.01:
        g = g.resize((int(g.width * scale), int(g.height * scale)))
    if alpha < 1:
        g = g.copy()
        g.putalpha(g.getchannel("A").point(lambda v: int(v * alpha)))
    base.alpha_composite(g, (int(cx - g.width / 2), int(cy - g.height / 2)))

def base(t):
    img = BG.convert("RGBA")
    p = pulse(t)
    paste_glow(img, G_BLUE, 1500 + 70 * math.sin(t * 0.2), 180, 1.05 + 0.04 * p, 0.5)
    paste_glow(img, G_PURPLE, 250 + 50 * math.cos(t * 0.18), 850, 0.9, 0.35)
    paste_glow(img, G_CYAN, 950 + 80 * math.sin(t * 0.1), 930, 0.7, 0.22)
    d = ImageDraw.Draw(img)
    for y in range(90, H, 64):
        for x in range(80, W, 64):
            a = 22 + int(10 * math.sin(t * 1.4 + x * 0.01 + y * 0.02))
            d.ellipse((x, y, x + 2, y + 2), fill=(59, 130, 246, a))
    return img

def shadow(layer, box, radius=24, blur=26, alpha=48, off=(0, 14)):
    w = box[2] - box[0] + 80
    h = box[3] - box[1] + 80
    im = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    ImageDraw.Draw(im).rounded_rectangle((40, 40, 40 + box[2] - box[0], 40 + box[3] - box[1]),
                                         radius=radius, fill=(15, 35, 70, alpha))
    im = im.filter(ImageFilter.GaussianBlur(blur))
    layer.alpha_composite(im, (int(box[0] - 40 + off[0]), int(box[1] - 40 + off[1])))

def card(layer, box, radius=24, fill=(255, 255, 255, 244),
         outline=(191, 219, 254, 255), width=2):
    shadow(layer, box, radius=radius)
    ImageDraw.Draw(layer).rounded_rectangle(box, radius=radius, fill=fill,
                                            outline=outline, width=width)

def pill(layer, x, y, s, f, color, alpha=255):
    d = ImageDraw.Draw(layer)
    w = tw(s, f)
    box = (x, y, x + w + 36, y + f.size + 16)
    d.rounded_rectangle(box, radius=(box[3] - box[1]) // 2,
                        fill=(*color, int(24 * alpha / 255)))
    d.text((x + 18, y + 5), s, font=f, fill=(*color, alpha))
    return box

def reveal(layer, x, y, s, f, fill, p, anchor="la"):
    p = clamp(p)
    if p <= 0:
        return
    w = max(4, int(tw(s, f) * p) + 4)
    tmp = Image.new("RGBA", (w, f.size + 30), (0, 0, 0, 0))
    ImageDraw.Draw(tmp).text((0, 0), s, font=f, fill=(*fill, 255))
    layer.alpha_composite(tmp, (int(x), int(y - 10)))

def flash(layer, t):
    a = int(24 * pulse(t, 9))
    if a:
        layer.alpha_composite(Image.new("RGBA", (W, H), (255, 255, 255, a)))

def scene_intro(layer, t):
    p = fade(t, 0, 10.2, 0.5)
    if p <= 0:
        return
    cx, cy = W // 2, 380
    b = pulse(t, 4)
    for i in range(3):
        r = int(160 + i * 54 + 10 * b)
        ImageDraw.Draw(layer).ellipse((cx - r, cy - r, cx + r, cy + r),
                                      outline=(37, 99, 235, int(95 * (1 - i / 3) * p)), width=2)
    sc = back(clamp(t / 0.9))
    size = int(225 * (0.62 + 0.38 * sc))
    layer.alpha_composite(ICON.resize((size, size)), (cx - size // 2, cy - size // 2))
    paste_glow(layer, G_BLUE, cx, cy + 10, 0.6 + 0.05 * b, 0.4 * p)
    title = "FLFHMCL"
    w = tw(title, F["hero"])
    reveal(layer, cx - w / 2, cy + 165, title, F["hero"], TEXT, smooth((t - 0.7) / 0.75))
    text(layer, cx, cy + 305, "为 HMCL 提供 Fabric 运行时", F["h2"], BLUE,
         anchor="ma", alpha=int(255 * smooth((t - 1.5) / 0.7)))
    text(layer, cx, cy + 380, "HMCL 原生没有扩展接口；FLFHMCL 用 Fabric 补上运行时", F["body"], SUB,
         anchor="ma", alpha=int(225 * smooth((t - 2.2) / 0.7)))
    tags = [("Fabric 运行时", BLUE), ("FLF API", PURPLE), ("跨版本维护", CYAN)]
    for i, (s, c) in enumerate(tags):
        app = back(clamp((t - 3.4 - i * 0.15) / 0.45))
        if app > 0:
            pill(layer, cx - 300 + i * 215, cy + 455, s, F["small"], c, int(255 * app))

def scene_problem(layer, t):
    p = fade(t, 9.8, 26.2, 0.5)
    if p <= 0:
        return
    text(layer, 130, 110, "为什么 HMCL 版本一换，组件就崩？", F["h1"], TEXT, alpha=int(255 * p))
    text(layer, 134, 225, "每个版本都在变，依赖、界面、启动方式全不一样", F["body"], SUB,
         alpha=int(220 * p))
    items = [
        ("版本分叉", "3.14、3.15、3.16、3.17 各有一套内部结构", BLUE),
        ("组件易碎", "为 3.16 编译的组件，换到 3.17 就启动失败", PURPLE),
        ("安装繁琐", "Java、Fabric、依赖库、资源，缺一个都开不了", ORANGE),
    ]
    for i, (title_s, sub, c) in enumerate(items):
        app = ease(clamp((t - 10.4 - i * 0.28) / 0.6))
        if app <= 0:
            continue
        x, y = 170 + i * 540, 380 - int((1 - app) * 45)
        box = (x, y, x + 490, y + 250)
        card(layer, box)
        d = ImageDraw.Draw(layer)
        d.ellipse((x + 30, y + 26, x + 86, y + 82), fill=(*c, 32))
        d.text((x + 47, y + 35), "!", font=F["h3"], fill=c)
        text(layer, x + 30, y + 108, title_s, F["h3"], TEXT, alpha=int(255 * app))
        text(layer, x + 30, y + 160, sub, F["small"], SUB, alpha=int(220 * app))
        d.rounded_rectangle((x + 30, y + 208, x + 450, y + 214), radius=3, fill=(226, 232, 240))
        d.rounded_rectangle((x + 30, y + 208, x + 30 + int(120 * app), y + 214), radius=3, fill=(*c, 170))
    text(layer, W // 2, 760, "FLFHMCL 要做的，是把这些差异挡在 HMCL 外面", F["h2"], BLUE,
         anchor="ma", alpha=int(240 * smooth((t - 14.0) / 0.7)))

def screenshot(layer, x, y, w, h, p, img=None):
    if p <= 0:
        return
    sc = 0.9 + 0.1 * ease(p)
    ww, hh = int(w * sc), int(h * sc)
    im = (img or HMCL_SCREEN).resize((ww, hh))
    mask = Image.new("L", (ww, hh), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, ww, hh), radius=20, fill=255)
    shadow(layer, (x, y, x + ww, y + hh), radius=20, blur=30, alpha=55, off=(0, 18))
    layer.paste(im, (int(x), int(y)), mask)
    ImageDraw.Draw(layer).rounded_rectangle((x, y, x + ww, y + hh), radius=20,
                                            outline=(147, 197, 253, 255), width=2)

def scene_core(layer, t):
    p = fade(t, 25.8, 44.2, 0.55)
    if p <= 0:
        return
    text(layer, 120, 95, "FLFHMCL", F["h2"], BLUE, alpha=int(230 * p))
    text(layer, 124, 160, "给 HMCL 一个 Fabric 运行时", F["h1"], TEXT, alpha=int(255 * p))
    text(layer, 128, 275, "让 Fabric mod 加载、用 Mixin 介入，并通过 FLF API 提供扩展接口", F["body"], SUB,
         alpha=int(220 * p))
    app = ease(clamp((t - 26.6) / 0.8))
    screenshot(layer, int(lerp(1250, 140, app)), 350, 1120, 684, p)

    points = [("Fabric 运行时", "HMCL 成为 Fabric GameProvider，mod 正常加载", BLUE),
              ("FLF API", "类似 Fabric API 的 HMCL 扩展层", PURPLE),
              ("跨版本维护", "抽象层隔离 HMCL 内部变化，不是目标", CYAN)]
    for i, (title_s, sub, c) in enumerate(points):
        a = smooth((t - 29.0 - i * 0.35) / 0.55)
        if a <= 0:
            continue
        x, y = 1270, 360 + i * 150
        d = ImageDraw.Draw(layer)
        d.rounded_rectangle((x, y, x + 510, y + 118), radius=20,
                            fill=(255, 255, 255, int(235 * a)),
                            outline=(*c, int(80 * a)), width=2)
        d.ellipse((x + 24, y + 34, x + 72, y + 82), fill=(*c, int(30 * a)))
        d.ellipse((x + 40, y + 50, x + 56, y + 66), fill=(*c, int(220 * a)))
        text(layer, x + 92, y + 22, title_s, F["small"], TEXT, alpha=int(255 * a))
        text(layer, x + 92, y + 65, sub, F["tiny"], SUB, alpha=int(215 * a))

def scene_versions(layer, t):
    p = fade(t, 43.8, 62.4, 0.55)
    if p <= 0:
        return
    text(layer, W // 2, 100, "一个 FLFHMCL，四个 HMCL 大版本", F["h1"], TEXT,
         anchor="ma", alpha=int(255 * p))
    text(layer, W // 2, 215, "跨版本兼容只是维护手段，不是 FLFHMCL 的目的", F["body"], SUB,
         anchor="ma", alpha=int(220 * p))
    versions = [("3.14", "3.14.1", BLUE), ("3.15", "3.15.3", PURPLE),
                ("3.16", "3.16.4", CYAN), ("3.17", "3.17.0", ORANGE)]
    for i, (major, patch, c) in enumerate(versions):
        a = back(clamp((t - 44.8 - i * 0.2) / 0.55))
        if a <= 0:
            continue
        x, y = 190 + i * 390, 390 - int((1 - a) * 65)
        d = ImageDraw.Draw(layer)
        d.rounded_rectangle((x, y, x + 330, y + 240), radius=26,
                            fill=(255, 255, 255, int(246 * a)),
                            outline=(*c, int(85 * a)), width=2)
        d.ellipse((x + 26, y + 24, x + 48, y + 46), fill=(*c, int(220 * a)))
        d.text((x + 62, y + 18), "HMCL", font=F["small"], fill=(*MUTED, int(255 * a)))
        d.text((x + 30, y + 70), major, font=F["num"], fill=(*TEXT, int(255 * a)))
        d.text((x + 30, y + 170), patch, font=F["small"], fill=(*SUB, int(230 * a)))
        d.rounded_rectangle((x + 190, y + 176, x + 306, y + 204), radius=14,
                            fill=(*GREEN, int(30 * a)))
        d.text((x + 206, y + 180), "已适配", font=F["tiny"], fill=(*GREEN, int(255 * a)))

    a = smooth((t - 48.0) / 0.6)
    if a > 0:
        d = ImageDraw.Draw(layer)
        d.rounded_rectangle((W // 2 - 380, 760, W // 2 + 380, 880), radius=60,
                            fill=(*BLUE, int(242 * a)))
        d.text((W // 2 - 340, 785), "FLFHMCL 2.1.6", font=F["h3"], fill=(255, 255, 255, int(255 * a)))
        d.text((W // 2 + 70, 792), "一个版本，全部覆盖", font=F["small"], fill=(224, 242, 254, int(255 * a)))
        d.text((W // 2 - 340, 830), "KnotClassLoader · Provider · Mixin", font=F["tiny"],
               fill=(191, 219, 254, int(230 * a)))

def scene_components(layer, t):
    p = fade(t, 62.0, 80.6, 0.55)
    if p <= 0:
        return
    text(layer, 120, 95, "FLF API：HMCL 的扩展层", F["h1"], TEXT, alpha=int(255 * p))
    text(layer, 124, 205, "类似 Fabric API：HMCL 本身没有扩展接口，FLF API 补上事件、注册表与抽象层", F["body"], SUB,
         alpha=int(220 * p))
    comps = [("FLF API", "事件总线 / 注册表", BLUE), ("UI API", "界面扩展", PURPLE),
             ("Account", "账号接入", CYAN), ("Download", "下载源", GREEN),
             ("Modpack", "整合包", ORANGE), ("flf-title", "标题标识", SKY)]
    for i, (name, sub, c) in enumerate(comps):
        a = ease(clamp((t - 63.0 - i * 0.12) / 0.5))
        if a <= 0:
            continue
        col, row = i % 3, i // 3
        x, y = 250 + col * 480, 380 + row * 230
        d = ImageDraw.Draw(layer)
        shadow(layer, (x, y, x + 410, y + 170), radius=20, alpha=40)
        d.rounded_rectangle((x, y, x + 410, y + 170), radius=20,
                            fill=(255, 255, 255, int(245 * a)),
                            outline=(*c, int(90 * a)), width=2)
        d.rounded_rectangle((x, y, x + 8, y + 170), radius=4, fill=(*c, int(220 * a)))
        d.text((x + 30, y + 24), name, font=F["h3"], fill=(*TEXT, int(255 * a)))
        d.text((x + 30, y + 92), sub, font=F["small"], fill=(*SUB, int(230 * a)))
        d.text((x + 30, y + 128), "0" + str(i + 1), font=F["tiny"], fill=(*c, int(180 * a)))

    a = smooth((t - 67.0) / 0.6)
    if a > 0:
        d = ImageDraw.Draw(layer)
        d.rounded_rectangle((W // 2 - 300, 850, W // 2 + 300, 930), radius=20,
                            fill=(*BLUE, int(24 * a)))
        d.text((W // 2, 868), "让 mod 不必绑死 HMCL 内部类，跨版本为 HMCL 提供服务", font=F["small"],
               fill=(*BLUE, int(255 * a)), anchor="ma")

def scene_runtime(layer, t):
    p = fade(t, 80.2, 96.4, 0.55)
    if p <= 0:
        return
    text(layer, 120, 95, "自带运行时，下载即用", F["h1"], TEXT, alpha=int(255 * p))
    text(layer, 124, 205, "Java、JavaFX、Fabric、依赖库，不再依赖本机环境", F["body"], SUB,
         alpha=int(220 * p))
    stack = [("Java 25", "运行时", BLUE), ("JavaFX", "界面库", PURPLE),
             ("Fabric Loader", "模组加载", CYAN), ("Libraries", "依赖库", GREEN)]
    for i, (name, sub, c) in enumerate(stack):
        a = ease(clamp((t - 81.0 - i * 0.16) / 0.5))
        if a <= 0:
            continue
        y = 370 + i * 110
        d = ImageDraw.Draw(layer)
        d.rounded_rectangle((520, y, 1400, y + 88), radius=18,
                            fill=(255, 255, 255, int(242 * a)),
                            outline=(*c, int(90 * a)), width=2)
        d.rounded_rectangle((520, y, 528, y + 88), radius=4, fill=(*c, int(220 * a)))
        d.text((556, y + 22), name, font=F["h3"], fill=(*TEXT, int(255 * a)))
        d.text((1240, y + 28), sub, font=F["small"], fill=(*c, int(230 * a)))

    a = smooth((t - 85.0) / 0.6)
    if a > 0:
        p1 = "自动检测"
        d = ImageDraw.Draw(layer)
        for i, s in enumerate(["自动检测", "自动下载", "自动解析", "自动修复"]):
            x = 510 + i * 240
            d.rounded_rectangle((x, 840, x + 210, 900), radius=30,
                                fill=(*BLUE, int(22 * a)))
            d.text((x + 38, 856), s, font=F["small"], fill=(*BLUE, int(255 * a)))

def scene_mclhr(layer, t):
    p = fade(t, 96.0, 112.4, 0.55)
    if p <= 0:
        return
    text(layer, 120, 95, "mclhr", F["h1"], PURPLE, alpha=int(255 * p))
    text(layer, 124, 210, "一份库，多个实例共用", F["h2"], TEXT, alpha=int(255 * p))
    text(layer, 128, 290, "自动解析、共享缓存，少下载，少占空间", F["body"], SUB,
         alpha=int(220 * p))

    jars = [(760, 350, BLUE, "JavaFX"), (760, 520, PURPLE, "Fabric"), (760, 690, CYAN, "依赖库")]
    for i, (x, y, c, label) in enumerate(jars):
        a = ease(clamp((t - 97.0 - i * 0.15) / 0.45))
        if a <= 0:
            continue
        d = ImageDraw.Draw(layer)
        d.rounded_rectangle((x, y, x + 110, y + 132), radius=14,
                            fill=(255, 255, 255, int(245 * a)),
                            outline=(*c, int(150 * a)), width=2)
        d.text((x + 30, y + 50), "JAR", font=F["small"], fill=(*c, int(255 * a)))
        d.text((x + 55, y + 142), label, font=F["tiny"], fill=(*SUB, int(230 * a)), anchor="ma")

    inst = [(1300, 400, "实例 A", GREEN), (1340, 570, "实例 B", GREEN), (1280, 720, "实例 C", GREEN)]
    for i, (x, y, name, c) in enumerate(inst):
        a = smooth((t - 98.0 - i * 0.2) / 0.5)
        if a <= 0:
            continue
        d = ImageDraw.Draw(layer)
        d.rounded_rectangle((x, y, x + 260, y + 90), radius=18,
                            fill=(255, 255, 255, int(240 * a)),
                            outline=(*PURPLE, int(90 * a)), width=2)
        d.ellipse((x + 24, y + 30, x + 54, y + 60), fill=(*c, int(220 * a)))
        d.text((x + 74, y + 28), name, font=F["small"], fill=(*TEXT, int(255 * a)))
        sx, sy = 870, 416 + i * 170
        d.line((sx, sy, x, y + 45), fill=(*PURPLE, int(90 * a)), width=2)
        q = (t * 0.8 + i * 0.3) % 1
        d.ellipse((lerp(sx, x, q) - 5, lerp(sy, y + 45, q) - 5,
                   lerp(sx, x, q) + 5, lerp(sy, y + 45, q) + 5),
                  fill=(*PURPLE, int(255 * a)))

    a = smooth((t - 100.5) / 0.5)
    if a > 0:
        d = ImageDraw.Draw(layer)
        d.rounded_rectangle((760, 855, 1260, 915), radius=30, fill=(*PURPLE, int(24 * a)))
        d.text((798, 870), "自动解析  ·  共享缓存  ·  跨实例复用", font=F["small"],
               fill=(*PURPLE, int(255 * a)))

def scene_crash(layer, t):
    p = fade(t, 112.0, 126.4, 0.55)
    if p <= 0:
        return
    text(layer, 120, 95, "崩溃了，也看得懂", F["h1"], TEXT, alpha=int(255 * p))
    text(layer, 124, 205, "报告自动保存，问题直接说人话", F["body"], SUB, alpha=int(220 * p))
    box = (170, 310, 1050, 820)
    card(layer, box)
    d = ImageDraw.Draw(layer)
    d.rounded_rectangle((170, 310, 1050, 372), radius=24, fill=(254, 242, 242, 255))
    d.ellipse((206, 330, 230, 354), fill=(239, 68, 68))
    d.ellipse((244, 330, 268, 354), fill=(251, 191, 36))
    d.ellipse((282, 330, 306, 354), fill=(34, 197, 94))
    d.text((206, 402), "HMCL 异常退出", font=F["h3"], fill=(*TEXT, int(255 * p)))
    d.text((206, 458), "检测到重复的 Fabric Loader 版本", font=F["small"], fill=(*ORANGE, int(255 * p)))
    d.text((206, 508), "退出代码：1", font=F["small"], fill=(*SUB, int(235 * p)))
    d.rounded_rectangle((206, 560, 1010, 568), radius=4, fill=(226, 232, 240))
    for i, line in enumerate(["建议更新到 FLFHMCL 2.1.6", "或点击下方按钮自动修复实例"]):
        d.text((206, 600 + i * 44), line, font=F["small"], fill=(*TEXT, int(225 * p)))
    d.rounded_rectangle((206, 710, 490, 772), radius=30, fill=(*BLUE, int(242 * p)))
    d.text((248, 727), "修复实例", font=F["small"], fill=(255, 255, 255, int(255 * p)))
    d.rounded_rectangle((520, 710, 710, 772), radius=30, fill=(241, 245, 249, int(255 * p)))
    d.text((560, 727), "查看报告", font=F["small"], fill=(*TEXT, int(255 * p)))

    a = smooth((t - 114.0) / 0.6)
    if a > 0:
        x, y = 1180, 360
        d = ImageDraw.Draw(layer)
        d.rounded_rectangle((x, y, x + 560, y + 330), radius=22,
                            fill=(255, 255, 255, int(245 * a)),
                            outline=(*GREEN, int(130 * a)), width=2)
        d.text((x + 34, y + 28), "crash-report", font=F["h3"], fill=(*TEXT, int(255 * a)))
        for i, line in enumerate([".hmcl/crash-report/", "flfhmcl-crash-2026...txt"]):
            d.text((x + 34, y + 105 + i * 56), line, font=F["mono"], fill=(*SUB, int(240 * a)))
        d.text((x + 34, y + 220), "已保存  ·  已复制诊断", font=F["tiny"], fill=(*GREEN, int(255 * a)))
        d.rounded_rectangle((x + 34, y + 260, x + 300, y + 302), radius=20,
                            fill=(*GREEN, int(28 * a)))
        d.text((x + 56, y + 266), "一键反馈", font=F["small"], fill=(*GREEN, int(255 * a)))

def scene_launcher(layer, t):
    p = fade(t, 126.0, 138.2, 0.5)
    if p <= 0:
        return
    text(layer, 120, 95, "FLFHMCLauncher：外壳，不是核心", F["h2"], BLUE, alpha=int(240 * p))
    text(layer, 124, 165, "环境配置 · 快速启动 · 版本隔离 · 缓存", F["h1"], TEXT, alpha=int(255 * p))
    text(layer, 128, 280, "不用它也能直接启动 FLFHMCL；它只是把常用操作包了一层", F["body"],
         SUB, alpha=int(220 * p))
    a = ease(clamp((t - 126.8) / 0.8))
    screenshot(layer, int(lerp(1350, 170, a)), 360, 1030, 714, p, LAUNCHER_SCREEN)
    points = [("环境配置", "Java、HMCL、FLFHMCL 统一准备", BLUE),
              ("快速启动", "桌面快捷方式，双击即玩", PURPLE),
              ("版本隔离 / 缓存", "实例独立，共享库复用", CYAN)]
    for i, (title_s, sub, c) in enumerate(points):
        aa = smooth((t - 128.8 - i * 0.3) / 0.5)
        if aa <= 0:
            continue
        x, y = 1280, 390 + i * 140
        d = ImageDraw.Draw(layer)
        d.rounded_rectangle((x, y, x + 510, y + 110), radius=18,
                            fill=(255, 255, 255, int(235 * aa)),
                            outline=(*c, int(80 * aa)), width=2)
        d.ellipse((x + 24, y + 30, x + 70, y + 76), fill=(*c, int(28 * aa)))
        d.text((x + 88, y + 20), title_s, font=F["small"], fill=(*TEXT, int(255 * aa)))
        d.text((x + 88, y + 58), sub, font=F["tiny"], fill=(*SUB, int(220 * aa)))

def scene_credits(layer, t):
    p = fade(t, 138.0, 150.2, 0.6)
    if p <= 0:
        return
    text(layer, 150, 110, "开源与参考", F["h2"], BLUE, alpha=int(255 * p))
    text(layer, 154, 205, "HMCL 原生没有扩展接口；FLFHMCL 用 Fabric 补上运行时，这些项目提供了基础", F["body"], SUB, alpha=int(220 * p))

    rows = [
        ("FLFHMCL", "github.com/HiBer2007/FLFHMCL", "LGPL-2.1", BLUE),
        ("mclhr", "github.com/HiBer2007/mclhr", "LGPL-2.1", PURPLE),
        ("HMCL", "github.com/HMCL-dev/HMCL", "GPL-3.0", CYAN),
        ("Fabric Loader", "fabricmc.net", "Apache-2.0", GREEN),
        ("Metadata", "metadata.hibernet.top", "FLF 组件源", ORANGE),
        ("CrashTracker", "github.com/HiBer2007/CrashTracker", "崩溃捕获", SKY),
    ]
    for i, (name, url, lic, c) in enumerate(rows):
        a = smooth((t - 139.0 - i * 0.12) / 0.45)
        if a <= 0:
            continue
        y = 310 + i * 95
        d = ImageDraw.Draw(layer)
        d.rounded_rectangle((150, y, 1770, y + 76), radius=14,
                            fill=(255, 255, 255, int(230 * a)),
                            outline=(226, 232, 240, int(255 * a)), width=1)
        d.rounded_rectangle((150, y, 158, y + 76), radius=4, fill=(*c, int(220 * a)))
        d.text((184, y + 18), name, font=F["small"], fill=(*TEXT, int(255 * a)))
        d.text((500, y + 20), url, font=F["tiny"], fill=(*SUB, int(235 * a)))
        d.text((1510, y + 20), lic, font=F["tiny"], fill=(*c, int(255 * a)))

    a = smooth((t - 145.0) / 0.5)
    if a > 0:
        d = ImageDraw.Draw(layer)
        d.rounded_rectangle((150, 900, 1120, 966), radius=18, fill=(*BLUE, int(22 * a)))
        d.text((178, 914), "Music: Dragonflame — Kirara Magic", font=F["small"],
               fill=(*BLUE, int(255 * a)))
        d.text((1180, 914), "FLFHMCL 2.1.6  ·  FLFHMCLauncher 可选", font=F["tiny"],
               fill=(*SUB, int(240 * a)))

def transition(layer, t):
    cuts = [9.8, 25.8, 43.8, 62.0, 80.2, 96.0, 112.0, 126.0, 138.0]
    a = 0
    for c in cuts:
        d = abs(t - c)
        if d < 0.16:
            a = max(a, int(105 * (1 - d / 0.16)))
    if a:
        layer.alpha_composite(Image.new("RGBA", (W, H), (255, 255, 255, a)))

def render(i):
    t = i / FPS
    img = base(t)
    scene_intro(img, t)
    scene_problem(img, t)
    scene_core(img, t)
    scene_versions(img, t)
    scene_components(img, t)
    scene_runtime(img, t)
    scene_mclhr(img, t)
    scene_crash(img, t)
    scene_launcher(img, t)
    scene_credits(img, t)
    flash(img, t)
    transition(img, t)
    d = ImageDraw.Draw(img)
    d.rectangle((0, H - 4, W, H), fill=(255, 255, 255, 255))
    d.rectangle((0, H - 4, int(W * t / DURATION), H), fill=(*BLUE, 220))
    flat = Image.alpha_composite(Image.new("RGBA", (W, H), (255, 255, 255, 255)), img)
    flat.convert("RGB").save(OUT / f"{i:05d}.jpg", quality=94)
    return i

def render_all():
    from multiprocessing import Pool
    total = int(DURATION * FPS)
    with Pool(processes=max(2, os.cpu_count() // 2)) as pool:
        for n, _ in enumerate(pool.imap_unordered(render, range(total), chunksize=8), 1):
            if n % 200 == 0:
                print(f"rendered {n}/{total}")

def main():
    render_all()
    subprocess.run([
        r"D:\Program Files\ffmpeg\bin\ffmpeg.exe", "-y",
        "-framerate", str(FPS), "-i", str(OUT / "%05d.jpg"),
        "-ss", "15", "-t", str(DURATION), "-i", r"K:\Music\Dragonflame - Kirara Magic.flac",
        "-filter_complex", "[1:a]afade=t=in:st=0:d=1.5,afade=t=out:st=146:d=4[a]",
        "-map", "0:v:0", "-map", "[a]",
        "-c:v", "libx264", "-preset", "medium", "-crf", "18",
        "-pix_fmt", "yuv420p", "-c:a", "aac", "-b:a", "192k",
        "-shortest", str(ROOT / "FLFHMCL-2分30秒-宣传片.mp4"),
    ], check=False)

if __name__ == "__main__":
    main()
