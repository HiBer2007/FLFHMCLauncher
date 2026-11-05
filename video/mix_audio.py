# -*- coding: utf-8 -*-
"""把 SAPI 旁白按时间点混入环境铺底。"""
import wave
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parent
DURATION = 88.0
SR = 44100
OFFSETS = [0.6, 6.7, 15.4, 25.5, 36.3, 47.9, 59.3, 69.7, 79.5]

def read_channel(path):
    with wave.open(str(path), "rb") as w:
        channels = w.getnchannels()
        data = w.readframes(w.getnframes())
        samples = list(struct.unpack("<%dh" % (len(data) // 2), data))
        if channels == 2:
            return samples[0::2]
        return samples

ambient = read_channel(ROOT / "audio.wav")

mix = [0.0] * int(DURATION * SR)
for i, p in enumerate(ambient):
    if i < len(mix):
        mix[i] += p * 0.32

for idx, start in enumerate(OFFSETS):
    voice = read_channel(ROOT / "voice" / f"{idx}.wav")
    base = int(start * SR)
    for j, p in enumerate(voice):
        k = base + j
        if k >= len(mix):
            break
        mix[k] += p * 0.95

with wave.open(str(ROOT / "narration.wav"), "wb") as w:
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(SR)
    frames = bytearray()
    for v in mix:
        p = max(-32767, min(32767, int(v)))
        frames += struct.pack("<h", p)
    w.writeframes(bytes(frames))
print("narration.wav", len(frames))
