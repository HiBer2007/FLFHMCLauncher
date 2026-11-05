import wave
import numpy as np

w = wave.open('music.wav', 'rb')
sr = w.getframerate()
x = np.frombuffer(w.readframes(w.getnframes()), dtype='<i2').astype(np.float32) / 32768
for s in range(0, 120, 5):
    seg = x[s * sr:(s + 5) * sr]
    print(s, round(float(np.sqrt(np.mean(seg ** 2))), 4))
