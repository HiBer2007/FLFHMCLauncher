import wave
import numpy as np

w = wave.open('music.wav', 'rb')
sr = w.getframerate()
x = np.frombuffer(w.readframes(w.getnframes()), dtype='<i2').astype(np.float32) / 32768
hop = 512
win = 1024
frames = 1 + (len(x) - win) // hop
energy = np.array([np.sqrt(np.mean(x[i * hop:i * hop + win] ** 2)) for i in range(frames)])
flux = np.maximum(0, np.diff(energy, prepend=energy[0]))
fps = sr / hop
centered = flux - flux.mean()
ac = np.correlate(centered, centered, 'full')[len(centered) - 1:]
lags = np.arange(len(ac)) / fps
mask = (lags > 0.3) & (lags < 1.0)
lag = lags[mask][np.argmax(ac[mask])]
print('fps', fps, 'bpm', round(60 / lag, 2), 'lag', round(lag, 4))
th = flux.mean() + flux.std() * 1.3
peaks = []
for i in range(1, len(flux) - 1):
    if flux[i] > th and flux[i] >= flux[i - 1] and flux[i] > flux[i + 1]:
        peaks.append(i / fps)
out = []
for p in peaks:
    if not out or p - out[-1] > 0.18:
        out.append(p)
print('peaks', len(out))
print([round(p, 2) for p in out[:60]])
