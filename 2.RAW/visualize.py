#!/usr/bin/env python3
import sys
import numpy as np
from PIL import Image

W, H = 640, 512
path = sys.argv[1] if len(sys.argv) > 1 else "raw16.bin"
data = np.fromfile(path, dtype=np.uint16)
n = data.size // (W * H)
frames = data[:n * W * H].reshape(n, H, W).astype(np.float32)
print(f"frames={n}")

# 1. Средний кадр
avg = frames.mean(axis=0)
lo, hi = np.percentile(avg, [0.5, 99.5])
gray = np.clip((avg - lo) * 255.0 / (hi - lo), 0, 255).astype(np.uint8)
Image.fromarray(gray).save("out_avg.png")
print("out_avg.png — средний кадр 8-бит")

# 2. Карта шума (per-pixel std)
noise = frames.std(axis=0)
print(f"noise: min={noise.min():.1f} max={noise.max():.1f} median={np.median(noise):.2f}")
noise_vis = np.clip(noise * 10, 0, 255).astype(np.uint8)
Image.fromarray(noise_vis).save("out_noise.png")
print("out_noise.png — карта шума (×10)")

# 3. Гистограмма шума
hist, edges = np.histogram(noise, bins=50)
print("\ntemporal noise distribution (count):")
for i in range(0, 50, 5):
    bar = "#" * int(hist[i] / max(hist.max(), 1) * 60)
    print(f"  {edges[i]:>6.1f}: {hist[i]:>6} {bar}")

# 4. "Ступеньки" шума
print(f"\np50={np.median(noise):.2f}  p90={np.percentile(noise,90):.2f}  p99={np.percentile(noise,99):.2f}")
print(f"NETD (p50) = {np.median(noise)/100:.3f} °C = {np.median(noise)*10:.1f} mK")
print(f"NETD (p90) = {np.percentile(noise,90)/100:.3f} °C = {np.percentile(noise,90)*10:.1f} mK")
