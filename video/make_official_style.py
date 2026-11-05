# -*- coding: utf-8 -*-
"""官方 DSH 风格：真实录屏 + 极简白底标题卡，100 秒。"""
import subprocess
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent
FF = r"D:\Program Files\ffmpeg\bin\ffmpeg.exe"
W, H = 1920, 1080
FONT_B = r"C:\Windows\Fonts\msyhbd.ttc"
FONT = r"C:\Windows\Fonts\msyh.ttc"
LOGO = ROOT.parent / "assets" / "icon" / "flfhmlauncher-256.png"

def card(path, title, sub="", tag="FLFHMCL"):
    img = Image.new("RGB", (W, H), (250, 251, 253))
    d = ImageDraw.Draw(img)
    # 细网格
    for x in range(0, W, 64):
        d.line((x, 0, x, H), fill=(238, 242, 248), width=1)
    for y in range(0, H, 64):
        d.line((0, y, W, y), fill=(238, 242, 248), width=1)
    logo = Image.open(LOGO).convert("RGBA").resize((96, 96))
    img.paste(logo, (140, 130), logo)
    d.text((252, 152), tag, font=ImageFont.truetype(FONT_B, 34), fill=(37, 99, 235))
    # 主标题居中偏左
    f = ImageFont.truetype(FONT_B, 96)
    d.text((140, 430), title, font=f, fill=(15, 23, 42))
    if sub:
        fs = ImageFont.truetype(FONT, 40)
        d.text((144, 560), sub, font=fs, fill=(71, 85, 105))
    d.line((140, 380, 280, 380), fill=(37, 99, 235), width=5)
    img.save(path)

def lower_third(text, path, sub=""):
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    f = ImageFont.truetype(FONT_B, 42)
    fs = ImageFont.truetype(FONT, 26)
    w = max(d.textlength(text, font=f), d.textlength(sub, font=fs) if sub else 0)
    d.rounded_rectangle((100, 900, 100 + w + 64, 1010), radius=18,
                        fill=(255, 255, 255, 238), outline=(203, 213, 225, 255), width=1)
    d.text((132, 916), text, font=f, fill=(15, 23, 42))
    if sub:
        d.text((134, 968), sub, font=fs, fill=(71, 85, 105))
    img.save(path)

def run(args):
    subprocess.run(args, check=False)

def video_segment(src, start, dur, out, text, sub="", crop=None):
    lower_third(text, ROOT / "_lt.png", sub)
    vf = []
    if crop:
        vf.append(f"crop={crop[0]}:{crop[1]}:{crop[2]}:{crop[3]}")
    vf += ["scale=1920:1080:force_original_aspect_ratio=increase",
           "crop=1920:1080", "setsar=1", "fps=30"]
    run([FF, "-y", "-ss", str(start), "-t", str(dur), "-i", str(src),
         "-i", str(ROOT / "_lt.png"),
         "-filter_complex", ",".join(vf) + "[v];[v][1:v]overlay=0:0[o]",
         "-map", "[o]", "-an", "-c:v", "libx264", "-preset", "medium",
         "-crf", "19", "-pix_fmt", "yuv420p", str(out)])

def image_segment(img, dur, out):
    run([FF, "-y", "-loop", "1", "-i", str(img), "-t", str(dur),
         "-vf", "zoompan=z='min(zoom+0.00035,1.06)':d=1:s=1920x1080:fps=30,format=yuv420p",
         "-c:v", "libx264", "-preset", "medium", "-crf", "19", str(out)])

def main():
    cards = [
        ("FLFHMCL", "给 HMCL 一个 Fabric 运行时", "HMCL 原生没有扩展接口", "FLFHMCL"),
        ("FLF API", "HMCL 的扩展层", "类似 Fabric API；让 mod 跨版本为 HMCL 提供服务", "FLF API"),
        ("mclhr", "一份库，多个实例共用", "自动解析 · 共享缓存 · 跨实例复用", "mclhr"),
        ("崩溃报告", "自动保存，一键修复", "不用再翻一长串日志", "FLFHMCL"),
        ("开源与参考", "FLFHMCL 2.1.6", "github.com/HiBer2007/FLFHMCL", "FLFHMCL"),
    ]
    imgs = []
    for i, (t, s, sub, tag) in enumerate(cards):
        p = ROOT / f"_card{i}.png"
        card(p, s, sub, tag)
        imgs.append(p)

    seg = ROOT / "_seg"
    seg.mkdir(exist_ok=True)
    # 真实录屏，白底标题卡穿插
    image_segment(imgs[0], 6, seg / "s0.mp4")
    video_segment(ROOT / "hmcl-demo.mp4", 0, 14, seg / "s1.mp4",
                  "HMCL 已被 FLFHMCL 接管", "标题栏标记 + Fabric 运行时", crop=(1544, 942, 0, 0))
    video_segment(ROOT / "launcher-demo.mp4", 0, 12, seg / "s2.mp4",
                  "FLFHMCLauncher：可选外壳", "环境配置 / 快速启动 / 版本隔离 / 缓存",
                  crop=(1060, 700, 30, 40))
    video_segment(ROOT / "hmcl-demo.mp4", 2, 12, seg / "s3.mp4",
                  "一个运行时，接管多个 HMCL", "3.14 / 3.15 / 3.16 / 3.17",
                  crop=(1100, 700, 0, 0))
    image_segment(imgs[1], 10, seg / "s4.mp4")
    video_segment(ROOT / "launcher-demo.mp4", 2, 12, seg / "s5.mp4",
                  "环境、实例、缓存，一次配好", "不用它也能直接启动 FLFHMCL",
                  crop=(1060, 700, 30, 40))
    image_segment(imgs[2], 10, seg / "s6.mp4")
    video_segment(ROOT / "launcher-demo.mp4", 0, 12, seg / "s7.mp4",
                  "快速启动器：双击即玩", "桌面快捷方式直接进入游戏",
                  crop=(1060, 700, 30, 40))
    image_segment(imgs[3], 8, seg / "s8.mp4")
    image_segment(imgs[4], 8, seg / "s9.mp4")

    concat = seg / "list.txt"
    with open(concat, "w", encoding="utf-8") as f:
        for i in range(10):
            f.write(f"file '{seg / ('s' + str(i) + '.mp4')}'\n")
    run([FF, "-y", "-f", "concat", "-safe", "0", "-i", str(concat),
         "-i", str(ROOT / "music_full.wav"),
         "-filter_complex", "[1:a]afade=t=out:st=96:d=4[a]",
         "-map", "0:v:0", "-map", "[a]", "-c:v", "copy", "-c:a", "aac",
         "-b:a", "192k", "-shortest", "-t", "100",
         str(ROOT / "FLFHMCL-官方风格-100秒.mp4")])

if __name__ == "__main__":
    main()
