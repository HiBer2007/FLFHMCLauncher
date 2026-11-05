# -*- coding: utf-8 -*-
"""FLFHMCL 90 秒快剪版：压缩停留、加速运镜、提高切点密度。"""
import os
import subprocess
from pathlib import Path

from PIL import Image, ImageDraw

import make_director as d

W, H, FPS = d.W, d.H, d.FPS
ROOT = Path(__file__).resolve().parent
OUT = ROOT / "fast_frames"
OUT.mkdir(parents=True, exist_ok=True)

# (快剪开始, 快剪结束, 原场景函数, 原场景开始, 原场景结束)
TIMELINE = [
    (0.0, 5.0, d.scene_intro, 0.0, 12.0),
    (5.0, 13.0, d.scene_problem, 12.0, 32.0),
    (13.0, 24.0, d.scene_core, 32.0, 56.0),
    (24.0, 33.0, d.scene_versions, 56.0, 78.0),
    (33.0, 44.0, d.scene_api, 78.0, 104.0),
    (44.0, 53.0, d.scene_runtime, 104.0, 124.0),
    (53.0, 62.0, d.scene_mclhr, 124.0, 146.0),
    (62.0, 70.0, d.scene_crash, 146.0, 164.0),
    (70.0, 81.0, d.scene_launcher, 164.0, 188.0),
    (81.0, 90.0, d.scene_credits, 188.0, 198.6666667),
]
DURATION = 90.0

def scene_at(t):
    for fs, fe, fn, os_, oe in TIMELINE:
        if fs <= t < fe:
            p = (t - fs) / (fe - fs)
            ot = os_ + p * (oe - os_)
            return fn(ot)
    return d.scene_credits(198.6)

def render(i):
    t = i / FPS
    img = scene_at(t)
    if img is None:
        img = d.b.base(t)
    # 快剪白色闪切
    for fs, fe, *_ in TIMELINE[1:]:
        dd = abs(t - fs)
        if dd < 0.10:
            a = int(95 * (1 - dd / 0.10))
            img.alpha_composite(Image.new("RGBA", (W, H), (255, 255, 255, a)))
            break
    d.b.flash(img, t)
    dr = ImageDraw.Draw(img)
    dr.rectangle((0, H - 4, W, H), fill=(255, 255, 255, 255))
    dr.rectangle((0, H - 4, int(W * t / DURATION), H), fill=(*d.BLUE, 230))
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
    audio = ROOT / "music_full.wav"
    subprocess.run([
        r"D:\Program Files\ffmpeg\bin\ffmpeg.exe", "-y",
        "-framerate", str(FPS), "-i", str(OUT / "%05d.jpg"),
        "-i", str(audio),
        "-filter_complex", "[1:a]afade=t=out:st=86:d=4[a]",
        "-map", "0:v:0", "-map", "[a]",
        "-c:v", "libx264", "-preset", "medium", "-crf", "18",
        "-pix_fmt", "yuv420p", "-c:a", "aac", "-b:a", "192k",
        "-shortest", "-t", str(DURATION),
        str(ROOT / "FLFHMCL-90秒-快剪版.mp4"),
    ], check=False)

if __name__ == "__main__":
    main()
