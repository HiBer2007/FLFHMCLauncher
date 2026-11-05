# -*- coding: utf-8 -*-
"""FLFHMCL 3:18 宣传片：完整音乐、镜头运动、启动器详细演示。"""
import math
import os
import subprocess
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

import make_film as b

W, H = b.W, b.H
FPS = 30
DURATION = 198.6666667
ROOT = Path(__file__).resolve().parent
OUT = ROOT / "director_frames"
OUT.mkdir(parents=True, exist_ok=True)

BLUE, PURPLE, CYAN = b.BLUE, b.PURPLE, b.CYAN
GREEN, ORANGE, TEXT, SUB = b.GREEN, b.ORANGE, b.TEXT, b.SUB
MUTED = b.MUTED
F = b.F
ICON = b.ICON
HMCL = b.HMCL_SCREEN
LAUNCHER = b.LAUNCHER_SCREEN

def clamp(v, a=0.0, b_=1.0):
    return max(a, min(b_, v))

def smooth(x):
    x = clamp(x)
    return x * x * (3 - 2 * x)

def ease(x):
    x = clamp(x)
    return 1 - (1 - x) ** 3

def fade(t, a, b_, f=0.5):
    if t < a or t > b_:
        return 0.0
    return smooth((t - a) / f) * smooth((b_ - t) / f)

def lerp(a, b_, p):
    return a + (b_ - a) * p

def pulse(t, sharp=5.0):
    # 与音乐节拍近似对齐：107.67 BPM
    return math.exp(-((t / 0.5573) % 1.0) * sharp)

def tw(s, f):
    return ImageDraw.Draw(Image.new("RGB", (8, 8))).textlength(s, font=f)

def txt(img, x, y, s, f, fill, anchor="la", alpha=255):
    b.text(img, x, y, s, f, fill, anchor=anchor, alpha=alpha)

def camera(img, zoom=1.0, cx=W / 2, cy=H / 2):
    if zoom <= 1.001:
        return img
    vw, vh = W / zoom, H / zoom
    left = clamp(cx - vw / 2, 0, W - vw)
    top = clamp(cy - vh / 2, 0, H - vh)
    crop = img.crop((int(left), int(top), int(left + vw), int(top + vh)))
    return crop.resize((W, H), Image.LANCZOS)

def world_bg(w=3000):
    img = Image.new("RGBA", (w, H), (249, 251, 255, 255))
    d = ImageDraw.Draw(img)
    for y in range(0, H, 64):
        for x in range(40, w, 64):
            d.ellipse((x, y, x + 2, y + 2), fill=(59, 130, 246, 22))
    return img

def screen(img, x, y, w, h, source=HMCL, radius=18):
    im = source.convert("RGB").resize((int(w), int(h)))
    mask = Image.new("L", (int(w), int(h)), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, w, h), radius=radius, fill=255)
    shadow = Image.new("RGBA", (int(w) + 100, int(h) + 100), (0, 0, 0, 0))
    ImageDraw.Draw(shadow).rounded_rectangle((50, 50, w + 50, h + 50), radius=radius,
                                             fill=(15, 35, 70, 55))
    shadow = shadow.filter(ImageFilter.GaussianBlur(30))
    img.alpha_composite(shadow, (int(x - 50), int(y - 40)))
    img.paste(im, (int(x), int(y)), mask)
    ImageDraw.Draw(img).rounded_rectangle((x, y, x + w, y + h), radius=radius,
                                          outline=(147, 197, 253, 255), width=2)

def scene_intro(t):
    img = b.base(t)
    p = fade(t, 0, 12.3, 0.5)
    cx, cy = W // 2, 410
    b_ = pulse(t, 4)
    for i in range(3):
        r = int(165 + i * 56 + 10 * b_)
        ImageDraw.Draw(img).ellipse((cx - r, cy - r, cx + r, cy + r),
                                    outline=(37, 99, 235, int(90 * (1 - i / 3) * p)), width=2)
    sc = b.back(clamp(t / 1.1))
    size = int(230 * (0.62 + 0.38 * sc))
    img.alpha_composite(ICON.resize((size, size)), (cx - size // 2, cy - size // 2))
    b.paste_glow(img, b.G_BLUE, cx, cy, 0.62, 0.4 * p)
    title = "FLFHMCL"
    twd = tw(title, F["hero"])
    b.reveal(img, cx - twd / 2, cy + 160, title, F["hero"], TEXT, smooth((t - 0.7) / 0.9))
    txt(img, cx, cy + 310, "为 HMCL 提供 Fabric 运行时", F["h2"], BLUE, "ma", int(255 * smooth((t - 1.6) / 0.8)))
    txt(img, cx, cy + 385, "HMCL 原生没有扩展接口；FLFHMCL 用 Fabric 补上运行时", F["body"], SUB,
        "ma", int(225 * smooth((t - 2.4) / 0.8)))
    tags = [("Fabric 运行时", BLUE), ("FLF API", PURPLE), ("跨版本维护", CYAN)]
    for i, (s, c) in enumerate(tags):
        a = b.back(clamp((t - 3.8 - i * 0.18) / 0.45))
        if a > 0:
            b.pill(img, cx - 315 + i * 220, cy + 465, s, F["small"], c, int(255 * a))
    return camera(img, 1.05 - 0.05 * ease(t / 10.0), W / 2, 470)

def scene_problem(t):
    p = fade(t, 12.0, 32.2, 0.5)
    if p <= 0:
        return None
    world = world_bg(3200)
    d = ImageDraw.Draw(world)
    # 背景大字
    d.text((90, 70), "NO EXTENSION", font=F["h1"], fill=(226, 237, 252))

    sockets = [
        (260, "没有插件接口", "HMCL 没有插件系统", BLUE, "PLUGIN"),
        (1180, "没有脚本 / 官方 API", "没有可供第三方接入的接口", PURPLE, "API"),
        (2100, "不是 Fabric 宿主", "Fabric mod 无处落脚", ORANGE, "FABRIC"),
    ]
    for i, (x, title, sub, color, tag) in enumerate(sockets):
        a = smooth((t - 13.0 - i * 0.3) / 0.6)
        if a <= 0:
            continue
        # 窗口轮廓
        wx, wy, ww, wh = x, 300, 520, 300
        d.rounded_rectangle((wx, wy, wx + ww, wy + wh), radius=18,
                            fill=(255, 255, 255, int(240 * a)),
                            outline=(203, 213, 225, int(255 * a)), width=2)
        d.rectangle((wx, wy, wx + ww, wy + 46), fill=(241, 245, 249, int(255 * a)))
        d.ellipse((wx + 18, wy + 14, wx + 32, wy + 28), fill=(239, 68, 68, int(220 * a)))
        d.ellipse((wx + 44, wy + 14, wx + 58, wy + 28), fill=(251, 191, 36, int(220 * a)))
        d.text((wx + 86, wy + 12), "Hello Minecraft! Launcher", font=F["tiny"], fill=(*SUB, int(230 * a)))
        # 空接口槽
        for k in range(3):
            cx = wx + 120 + k * 140
            cy = wy + 160
            d.ellipse((cx - 34, cy - 34, cx + 34, cy + 34), outline=(*color, int(130 * a)), width=3)
            d.line((cx - 16, cy, cx + 16, cy), fill=(*color, int(160 * a)), width=3)
            # 尝试插入的插头
            q = (t * 0.7 + k * 0.3) % 1
            yy = cy - 90 + 40 * q
            d.rounded_rectangle((cx - 10, yy, cx + 10, yy + 30), radius=5,
                                fill=(*color, int(180 * a)))
        txt(world, wx, wy + 345, title, F["h2"], TEXT, alpha=int(255 * a))
        txt(world, wx, wy + 415, sub, F["small"], SUB, alpha=int(225 * a))

    # 镜头横移
    camx = smooth(clamp((t - 12.0) / 19.0)) * 1050
    shot = world.crop((int(camx), 0, int(camx) + W, H))
    if t > 30:
        shot = camera(shot, 1.04, W / 2, 540)
    # 结尾结论固定在画面内，不跟着世界滚动
    a = smooth((t - 28.0) / 1.0)
    if a > 0:
        ImageDraw.Draw(shot).rounded_rectangle((300, 860, 1620, 990), radius=36,
                                               fill=(255, 255, 255, int(225 * a)))
        txt(shot, 340, 880, "所以，FLFHMCL 先给它一个 Fabric 运行时。", F["h2"], BLUE,
            alpha=int(255 * a))
    return shot

def scene_core(t):
    img = b.base(t)
    p = fade(t, 31.8, 56.2, 0.5)
    if p <= 0:
        return None
    txt(img, 120, 95, "FLFHMCL", F["h2"], BLUE, alpha=int(230 * p))
    txt(img, 124, 160, "给 HMCL 一个 Fabric 运行时", F["h1"], TEXT, alpha=int(255 * p))
    txt(img, 128, 278, "让 Fabric mod 加载、用 Mixin 介入，并通过 FLF API 提供扩展接口", F["body"], SUB,
        alpha=int(220 * p))

    app = ease(clamp((t - 32.6) / 1.0))
    x = int(lerp(1280, 130, app))
    screen(img, x, 360, 1120, 684)
    # 运行时地基
    a = smooth((t - 36.5) / 1.0)
    if a > 0:
        d = ImageDraw.Draw(img)
        d.rounded_rectangle((120, 955, 1260, 1010), radius=26, fill=(*BLUE, int(235 * a)))
        d.text((160, 970), "FLFHMCL  Fabric Runtime", font=F["small"], fill=(255, 255, 255, int(255 * a)))
        d.text((900, 972), "KnotClassLoader", font=F["tiny"], fill=(191, 219, 254, int(240 * a)))

    # 无卡片：浮动标注
    notes = [("Fabric 运行时", "HMCL 成为 Fabric GameProvider", BLUE, 36.0),
             ("FLF API", "类似 Fabric API 的 HMCL 扩展层", PURPLE, 39.0),
             ("跨版本维护", "抽象层隔离内部变化，不是目标", CYAN, 42.0)]
    for i, (title, sub, c, st) in enumerate(notes):
        a = smooth((t - st) / 0.6)
        if a <= 0:
            continue
        nx, ny = 1290, 390 + i * 150
        d = ImageDraw.Draw(img)
        d.line((1200, ny + 30, nx - 20, ny + 30), fill=(*c, int(150 * a)), width=2)
        d.ellipse((nx - 18, ny + 24, nx - 6, ny + 36), fill=(*c, int(255 * a)))
        txt(img, nx, ny, title, F["h3"], c, alpha=int(255 * a))
        txt(img, nx, ny + 55, sub, F["tiny"], SUB, alpha=int(215 * a))
    return camera(img, 1.05, 640, 520)

def scene_versions(t):
    img = b.base(t)
    p = fade(t, 55.8, 78.4, 0.5)
    if p <= 0:
        return None
    txt(img, W // 2, 90, "一个 FLFHMCL，多个 HMCL", F["h1"], TEXT, "ma", int(255 * p))
    txt(img, W // 2, 195, "跨版本是维护手段，不是目的", F["body"], SUB, "ma", int(220 * p))

    # 胶片
    scroll = (t - 56.0) * 55
    labels = [("3.14", "3.14.1", BLUE), ("3.15", "3.15.3", PURPLE),
              ("3.16", "3.16.4", CYAN), ("3.17", "3.17.0", ORANGE)]
    for i, (major, patch, c) in enumerate(labels):
        x = 240 + i * 470 - scroll
        if x < -420 or x > W + 60:
            continue
        w, h = 380, 250
        y = 360 + int(18 * math.sin(t * 0.8 + i))
        d = ImageDraw.Draw(img)
        d.rounded_rectangle((x, y, x + w, y + h), radius=24,
                            fill=(255, 255, 255, 245), outline=(*c, 70), width=2)
        d.text((x + 30, y + 24), "HMCL", font=F["small"], fill=(*MUTED, 255))
        d.text((x + 30, y + 70), major, font=F["num"], fill=(*TEXT, 255))
        d.text((x + 30, y + 180), patch, font=F["small"], fill=(*SUB, 230))
        d.rounded_rectangle((x + 210, y + 184, x + 340, y + 216), radius=16,
                            fill=(*GREEN, 28))
        d.text((x + 230, y + 190), "已适配", font=F["tiny"], fill=(*GREEN, 255))

    a = smooth((t - 70.0) / 0.7)
    if a > 0:
        d = ImageDraw.Draw(img)
        d.rounded_rectangle((W // 2 - 390, 790, W // 2 + 390, 900), radius=55,
                            fill=(*BLUE, int(238 * a)))
        d.text((W // 2 - 350, 812), "FLFHMCL 2.1.6", font=F["h3"], fill=(255, 255, 255, int(255 * a)))
        d.text((W // 2 + 120, 820), "一个版本，全部覆盖", font=F["small"], fill=(224, 242, 254, int(255 * a)))
    return img

def scene_api(t):
    img = b.base(t)
    p = fade(t, 78.0, 104.3, 0.5)
    if p <= 0:
        return None
    txt(img, 130, 90, "FLF API：HMCL 的扩展层", F["h1"], TEXT, alpha=int(255 * p))
    txt(img, 134, 200, "类似 Fabric API；HMCL 本身没有扩展接口，FLF API 补上抽象层", F["body"], SUB,
        alpha=int(220 * p))

    cx = 960
    # 能力脊柱
    d = ImageDraw.Draw(img)
    d.line((cx, 880, cx, 300), fill=(*BLUE, int(60 * p)), width=3)
    nodes = [("事件总线", "生命周期 / 实例 / 下载", BLUE),
             ("注册表", "整合包 / 启动方式 / 下载源", PURPLE),
             ("UI API", "侧栏 / 页面 / 设置区", CYAN),
             ("Account", "账号类型与登录桥", GREEN),
             ("Download", "下载源与镜像", ORANGE),
             ("Modpack", "整合包格式", b.SKY)]
    # 镜头升降：节点整体上移
    shift = (t - 78.0) * 12
    for i, (name, sub, c) in enumerate(nodes):
        y = 760 - i * 95 + 30 * math.sin(t * 0.7 + i) - shift * 0.35
        if y < 250 or y > 930:
            continue
        a = smooth((t - 79.0 - i * 0.25) / 0.5)
        if a <= 0:
            continue
        left = i % 2 == 0
        x = cx - 470 if left else cx + 80
        d.line((cx, y, x + (390 if left else 0), y), fill=(*c, int(100 * a)), width=2)
        d.ellipse((cx - 6, y - 6, cx + 6, y + 6), fill=(*c, int(255 * a)))
        d.text((x, y - 34), name, font=F["h3"], fill=(*TEXT, int(255 * a)))
        d.text((x, y + 8), sub, font=F["tiny"], fill=(*SUB, int(225 * a)))

    a = smooth((t - 88.0) / 0.7)
    if a > 0:
        d.rounded_rectangle((380, 960, 1540, 1020), radius=30, fill=(*BLUE, int(20 * a)))
        d.text((430, 974), "让 mod 不必绑死 HMCL 内部类，跨版本为 HMCL 提供服务", font=F["small"],
               fill=(*BLUE, int(255 * a)))
    return camera(img, 1.04, cx, 560)

def scene_runtime(t):
    img = b.base(t)
    p = fade(t, 103.8, 124.3, 0.5)
    if p <= 0:
        return None
    txt(img, 130, 90, "自带运行时，下载即用", F["h1"], TEXT, alpha=int(255 * p))
    txt(img, 134, 200, "Java、JavaFX、Fabric、依赖库，不再依赖本机环境", F["body"], SUB,
        alpha=int(220 * p))
    layers = [("Java 25", "运行时", BLUE), ("JavaFX", "界面库", PURPLE),
              ("Fabric Loader", "模组加载", CYAN), ("Libraries", "依赖库", GREEN)]
    d = ImageDraw.Draw(img)
    # 镜头下降：层相对于时间上移
    for i, (name, sub, c) in enumerate(layers):
        y = 380 + i * 125 - (t - 104.0) * 8
        a = smooth((t - 104.8 - i * 0.25) / 0.6)
        if a <= 0:
            continue
        d.rounded_rectangle((500, y, 1420, y + 92), radius=20,
                            fill=(255, 255, 255, int(238 * a)),
                            outline=(*c, int(80 * a)), width=2)
        d.rounded_rectangle((500, y, 510, y + 92), radius=5, fill=(*c, int(220 * a)))
        d.text((552, y + 24), name, font=F["h3"], fill=(*TEXT, int(255 * a)))
        d.text((1240, y + 30), sub, font=F["small"], fill=(*c, int(235 * a)))

    a = smooth((t - 110.0) / 0.7)
    if a > 0:
        for i, s in enumerate(["自动检测", "自动下载", "自动解析", "自动修复"]):
            x = 500 + i * 245
            d.rounded_rectangle((x, 900, x + 215, 960), radius=30, fill=(*BLUE, int(20 * a)))
            d.text((x + 40, 916), s, font=F["small"], fill=(*BLUE, int(255 * a)))
    return img

def scene_mclhr(t):
    img = b.base(t)
    p = fade(t, 124.0, 146.3, 0.5)
    if p <= 0:
        return None
    txt(img, 120, 95, "mclhr", F["h1"], PURPLE, alpha=int(255 * p))
    txt(img, 124, 210, "一份库，多个实例共用", F["h2"], TEXT, alpha=int(255 * p))
    txt(img, 128, 290, "底层库解析技术手段，不是产品核心", F["body"], SUB, alpha=int(220 * p))

    d = ImageDraw.Draw(img)
    jars = [(650, 360, BLUE, "JavaFX"), (770, 520, PURPLE, "Fabric"), (650, 680, CYAN, "依赖库")]
    for i, (x, y, c, label) in enumerate(jars):
        a = smooth((t - 125.0 - i * 0.2) / 0.6)
        if a <= 0:
            continue
        d.rounded_rectangle((x, y, x + 120, y + 140), radius=14,
                            fill=(255, 255, 255, int(242 * a)),
                            outline=(*c, int(150 * a)), width=2)
        d.text((x + 38, y + 52), "JAR", font=F["small"], fill=(*c, int(255 * a)))
        d.text((x + 60, y + 152), label, font=F["tiny"], fill=(*SUB, int(230 * a)), anchor="ma")
        d.line((x + 120, y + 70, 980, 560), fill=(*c, int(55 * a)), width=2)

    for i, (x, y, name) in enumerate([(1300, 400, "实例 A"), (1340, 570, "实例 B"), (1280, 720, "实例 C")]):
        a = smooth((t - 126.0 - i * 0.2) / 0.5)
        if a <= 0:
            continue
        d.rounded_rectangle((x, y, x + 260, y + 90), radius=16,
                            fill=(255, 255, 255, int(238 * a)),
                            outline=(*PURPLE, int(80 * a)), width=2)
        d.ellipse((x + 24, y + 30, x + 54, y + 60), fill=(*GREEN, int(220 * a)))
        d.text((x + 74, y + 28), name, font=F["small"], fill=(*TEXT, int(255 * a)))
        d.line((980, 560, x, y + 45), fill=(*PURPLE, int(80 * a)), width=2)

    a = smooth((t - 132.0) / 0.6)
    if a > 0:
        d.rounded_rectangle((700, 900, 1220, 960), radius=30, fill=(*PURPLE, int(22 * a)))
        d.text((740, 914), "自动解析 · 共享缓存 · 跨实例复用", font=F["small"],
               fill=(*PURPLE, int(255 * a)))
    return camera(img, 1.05, 900, 560)

def scene_crash(t):
    img = b.base(t)
    p = fade(t, 145.8, 164.2, 0.5)
    if p <= 0:
        return None
    shake = 0
    if t < 148:
        shake = int(5 * math.sin(t * 45) * (1 - (t - 146) / 2))
    txt(img, 120 + shake, 95, "崩溃了，也看得懂", F["h1"], TEXT, alpha=int(255 * p))
    txt(img, 124 + shake, 205, "报告自动保存，问题直接说人话", F["body"], SUB, alpha=int(220 * p))
    screen(img, 150 + shake, 330, 900, 560)
    # 异常蒙层
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((170 + shake, 330, 1030 + shake, 500), radius=18,
                        fill=(254, 242, 242, 238))
    d.text((210 + shake, 360), "HMCL 异常退出", font=F["h3"], fill=(*TEXT, 255))
    d.text((210 + shake, 420), "检测到重复的 Fabric Loader 版本", font=F["small"], fill=(*ORANGE, 255))
    d.text((210 + shake, 465), "退出代码：1", font=F["small"], fill=(*SUB, 235))

    a = smooth((t - 149.0) / 0.6)
    if a > 0:
        x, y = 1120, 350
        d.rounded_rectangle((x, y, x + 600, y + 360), radius=22,
                            fill=(255, 255, 255, int(245 * a)),
                            outline=(*GREEN, int(120 * a)), width=2)
        d.text((x + 34, y + 28), "crash-report", font=F["h3"], fill=(*TEXT, int(255 * a)))
        for i, line in enumerate([".hmcl/crash-report/", "flfhmcl-crash-2026...txt"]):
            d.text((x + 34, y + 110 + i * 58), line, font=F["mono"], fill=(*SUB, int(240 * a)))
        d.text((x + 34, y + 235), "已保存  ·  已复制诊断", font=F["tiny"], fill=(*GREEN, int(255 * a)))
        d.rounded_rectangle((x + 34, y + 275, x + 310, y + 325), radius=22, fill=(*GREEN, int(30 * a)))
        d.text((x + 56, y + 284), "一键反馈", font=F["small"], fill=(*GREEN, int(255 * a)))

    # 修复按钮
    a = smooth((t - 155.0) / 0.6)
    if a > 0:
        d.rounded_rectangle((170, 610, 470, 676), radius=32, fill=(*BLUE, int(240 * a)))
        d.text((220, 626), "修复实例", font=F["small"], fill=(255, 255, 255, int(255 * a)))
    return camera(img, 1.03, 700, 560)

def scene_launcher(t):
    img = b.base(t)
    p = fade(t, 163.8, 188.4, 0.45)
    if p <= 0:
        return None
    txt(img, 110, 85, "FLFHMCLauncher：外壳，不是核心", F["h2"], BLUE, alpha=int(240 * p))
    txt(img, 114, 155, "环境配置 · 快速启动 · 版本隔离 · 缓存", F["h1"], TEXT, alpha=int(255 * p))

    # 24 秒拆成四段
    phase = t - 164.0
    if phase < 7:  # 总览
        txt(img, 114, 265, "不用它也能直接启动 FLFHMCL；它只是把常用操作包了一层", F["body"], SUB,
            alpha=int(220 * p))
        a = ease(clamp(phase / 1.0))
        screen(img, int(lerp(1350, 520, a)), 330, 900, 624, LAUNCHER)
        txt(img, 520, 980, "一个窗口，管理所有实例", F["small"], SUB, alpha=int(200 * p))
    elif phase < 13:  # 环境配置
        txt(img, 114, 265, "Java、HMCL、FLFHMCL、Fabric 一次配好", F["body"], SUB, alpha=int(220 * p))
        screen(img, 520, 330, 900, 624, LAUNCHER)
        # 浮层配置行
        rows = [("Java 运行时", "自动下载 / 本机检测"), ("HMCL", "3.14 / 3.15 / 3.16"),
                ("FLFHMCL", "2.1.6"), ("Fabric Loader", "0.19.3+")]
        for i, (k, v) in enumerate(rows):
            a = smooth((phase - 7.2 - i * 0.18) / 0.4)
            if a <= 0:
                continue
            x, y = 1450, 360 + i * 105
            d = ImageDraw.Draw(img)
            d.line((1420, y + 30, x - 10, y + 30), fill=(*BLUE, int(100 * a)), width=1)
            d.text((x, y), k, font=F["small"], fill=(*SUB, int(255 * a)))
            d.text((x, y + 36), v, font=F["h3"], fill=(*TEXT, int(255 * a)))
    elif phase < 18:  # 快速启动
        txt(img, 114, 265, "为实例创建快速启动器，双击直接进入游戏", F["body"], SUB, alpha=int(220 * p))
        # 实例卡片放大
        d = ImageDraw.Draw(img)
        a = smooth((phase - 13.0) / 0.5)
        d.rounded_rectangle((430, 380, 1260, 560), radius=24,
                            fill=(255, 255, 255, int(242 * a)),
                            outline=(*BLUE, int(120 * a)), width=2)
        d.text((470, 410), "1.21.1-NeoForge", font=F["h3"], fill=(*TEXT, int(255 * a)))
        d.text((470, 470), "HMCL 3.16.4 · 已准备好", font=F["small"], fill=(*SUB, int(230 * a)))
        d.rounded_rectangle((1010, 440, 1200, 505), radius=30, fill=(*BLUE, int(240 * a)))
        d.text((1040, 456), "启动", font=F["small"], fill=(255, 255, 255, int(255 * a)))
        # 菜单 + 快速启动
        a2 = smooth((phase - 14.0) / 0.5)
        if a2 > 0:
            d.rounded_rectangle((900, 575, 1260, 700), radius=16, fill=(255, 255, 255, int(245 * a2)),
                                outline=(226, 232, 240, 255), width=1)
            d.text((930, 592), "更多", font=F["tiny"], fill=(*MUTED, int(255 * a2)))
            d.text((930, 630), "创建快速启动器", font=F["small"], fill=(*BLUE, int(255 * a2)))
            d.text((930, 665), "打开实例目录", font=F["small"], fill=(*TEXT, int(220 * a2)))
        a3 = smooth((phase - 15.0) / 0.5)
        if a3 > 0:
            d.rounded_rectangle((1320, 500, 1700, 690), radius=24,
                                fill=(255, 255, 255, int(242 * a3)),
                                outline=(*GREEN, int(100 * a3)), width=2)
            d.text((1350, 525), "FLFQuickStart", font=F["h3"], fill=(*TEXT, int(255 * a3)))
            d.text((1350, 585), "桌面快捷方式", font=F["small"], fill=(*SUB, int(230 * a3)))
            d.text((1350, 630), "双击 → 直接启动", font=F["small"], fill=(*GREEN, int(255 * a3)))
    else:  # 版本隔离 / 缓存
        txt(img, 114, 265, "实例独立；共享库复用，少下载、少占空间", F["body"], SUB, alpha=int(220 * p))
        d = ImageDraw.Draw(img)
        for i, (name, c) in enumerate([("实例 A", BLUE), ("实例 B", PURPLE), ("实例 C", CYAN)]):
            x = 300 + i * 450
            a = smooth((phase - 18.0 - i * 0.15) / 0.5)
            if a <= 0:
                continue
            d.rounded_rectangle((x, 380, x + 300, 620), radius=20,
                                fill=(255, 255, 255, int(238 * a)),
                                outline=(*c, int(80 * a)), width=2)
            d.text((x + 26, 405), name, font=F["h3"], fill=(*TEXT, int(255 * a)))
            d.text((x + 26, 465), ".minecraft", font=F["tiny"], fill=(*SUB, int(230 * a)))
            d.text((x + 26, 500), "mods / config", font=F["tiny"], fill=(*SUB, int(200 * a)))
            d.rounded_rectangle((x + 26, 550, x + 274, 560), radius=5, fill=(*c, int(80 * a)))
            d.line((x + 150, 620, 950, 720), fill=(*c, int(60 * a)), width=2)
        a = smooth((phase - 19.0) / 0.5)
        if a > 0:
            d.rounded_rectangle((400, 720, 1500, 840), radius=40, fill=(*BLUE, int(22 * a)))
            d.text((450, 745), "mclhr 共享库", font=F["h3"], fill=(*BLUE, int(255 * a)))
            d.text((450, 795), "JavaFX / Fabric / 依赖，只存一份", font=F["small"], fill=(*SUB, int(255 * a)))
    return img

def scene_credits(t):
    img = b.base(t)
    p = fade(t, 187.8, 198.8, 0.6)
    if p <= 0:
        return None
    txt(img, 150, 90, "开源与参考", F["h2"], BLUE, alpha=int(255 * p))
    txt(img, 154, 175, "HMCL 原生没有扩展接口；FLFHMCL 用 Fabric 补上运行时", F["body"], SUB,
        alpha=int(220 * p))

    rows = [
        ("FLFHMCL", "github.com/HiBer2007/FLFHMCL", "LGPL-2.1", BLUE),
        ("mclhr", "github.com/HiBer2007/mclhr", "LGPL-2.1", PURPLE),
        ("HMCL", "github.com/HMCL-dev/HMCL", "GPL-3.0", CYAN),
        ("Fabric Loader", "fabricmc.net", "Apache-2.0", GREEN),
        ("Metadata", "metadata.hibernet.top", "组件源", ORANGE),
        ("CrashTracker", "github.com/HiBer2007/CrashTracker", "崩溃捕获", b.SKY),
    ]
    scroll = max(0, (t - 189.0) * 8)
    for i, (name, url, lic, c) in enumerate(rows):
        a = smooth((t - 188.8 - i * 0.1) / 0.45)
        if a <= 0:
            continue
        y = 260 + i * 92 - scroll
        d = ImageDraw.Draw(img)
        d.rounded_rectangle((150, y, 1770, y + 72), radius=14,
                            fill=(255, 255, 255, int(228 * a)),
                            outline=(226, 232, 240, 255), width=1)
        d.rounded_rectangle((150, y, 157, y + 72), radius=4, fill=(*c, int(220 * a)))
        d.text((182, y + 16), name, font=F["small"], fill=(*TEXT, int(255 * a)))
        d.text((500, y + 18), url, font=F["tiny"], fill=(*SUB, int(235 * a)))
        d.text((1520, y + 18), lic, font=F["tiny"], fill=(*c, int(255 * a)))

    a = smooth((t - 193.0) / 0.5)
    if a > 0:
        d = ImageDraw.Draw(img)
        d.rounded_rectangle((150, 900, 1140, 966), radius=18, fill=(*BLUE, int(20 * a)))
        d.text((180, 914), "Music: Dragonflame — Kirara Magic", font=F["small"],
               fill=(*BLUE, int(255 * a)))
        d.text((1180, 914), "FLFHMCL 2.1.6", font=F["tiny"], fill=(*SUB, int(240 * a)))
    return camera(img, 1.02, 960, 540)

def transition(img, t):
    cuts = [12.0, 32.0, 56.0, 78.0, 104.0, 124.0, 146.0, 164.0, 188.0]
    a = 0
    for c in cuts:
        d = abs(t - c)
        if d < 0.14:
            a = max(a, int(90 * (1 - d / 0.14)))
    if a:
        img.alpha_composite(Image.new("RGBA", (W, H), (255, 255, 255, a)))

def render(i):
    t = i / FPS
    if t < 12:
        img = scene_intro(t)
    elif t < 32:
        img = scene_problem(t)
    elif t < 56:
        img = scene_core(t)
    elif t < 78:
        img = scene_versions(t)
    elif t < 104:
        img = scene_api(t)
    elif t < 124:
        img = scene_runtime(t)
    elif t < 146:
        img = scene_mclhr(t)
    elif t < 164:
        img = scene_crash(t)
    elif t < 188:
        img = scene_launcher(t)
    else:
        img = scene_credits(t)
    if img is None:
        img = b.base(t)
    b.flash(img, t)
    transition(img, t)
    d = ImageDraw.Draw(img)
    d.rectangle((0, H - 4, W, H), fill=(255, 255, 255, 255))
    d.rectangle((0, H - 4, int(W * t / DURATION), H), fill=(*BLUE, 220))
    flat = Image.alpha_composite(Image.new("RGBA", (W, H), (255, 255, 255, 255)), img)
    flat.convert("RGB").save(OUT / f"{i:05d}.jpg", quality=93)
    return i

def main():
    from multiprocessing import Pool
    total = int(DURATION * FPS)
    with Pool(processes=max(2, os.cpu_count() // 2)) as pool:
        for n, _ in enumerate(pool.imap_unordered(render, range(total), chunksize=8), 1):
            if n % 200 == 0:
                print(f"rendered {n}/{total}")
    subprocess.run([
        r"D:\Program Files\ffmpeg\bin\ffmpeg.exe", "-y",
        "-framerate", str(FPS), "-i", str(OUT / "%05d.jpg"),
        "-i", r"K:\Music\Dragonflame - Kirara Magic.flac",
        "-filter_complex", "[1:a]afade=t=out:st=194.6:d=4[a]",
        "-map", "0:v:0", "-map", "[a]",
        "-c:v", "libx264", "-preset", "medium", "-crf", "18",
        "-pix_fmt", "yuv420p", "-c:a", "aac", "-b:a", "192k",
        "-shortest", "-t", str(DURATION),
        str(ROOT / "FLFHMCL-完整版-宣传片.mp4"),
    ], check=False)

if __name__ == "__main__":
    main()
