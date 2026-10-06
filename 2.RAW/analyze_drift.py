#!/usr/bin/env python3
import sys
import numpy as np

W, H = 640, 512
path = sys.argv[1] if len(sys.argv) > 1 else "raw16.bin"
data = np.fromfile(path, dtype=np.uint16)
n = data.size // (W * H)
if n == 0:
    sys.exit(f"{path}: too small")

frames = data[:n * W * H].reshape(n, H, W).astype(np.float32)
print(f"frames={n}  shape={frames.shape}  bytes={data.nbytes}")

means = frames.mean(axis=(1, 2))
stds  = frames.std(axis=(1, 2))
mins  = frames.min(axis=(1, 2))
maxs  = frames.max(axis=(1, 2))

print("\nidx     mean       std       min      max     T_mean(°C)")
step = max(1, n // 25)
for i in range(0, n, step):
    T = (means[i] - 10000) / 100.0
    print(f"{i:>5}  {means[i]:>8.1f}  {stds[i]:>8.1f}  {mins[i]:>7.0f}  {maxs[i]:>7.0f}  {T:>8.2f}")

d = np.abs(np.diff(means))
jumps = np.where(d > 200)[0]
print(f"\nshutter-like jumps >200 count: {len(jumps)}")
for j in jumps[:30]:
    print(f"  frame {j:>4} -> {j+1:>4}:  {means[j]:>7.0f} -> {means[j+1]:>7.0f}   ΔT={-(means[j]-means[j+1])/100:>6.2f} °C")

# стабильные участки между скачками
segs = []
prev = 0
for j in list(jumps) + [n-1]:
    if j - prev > 20:
        segs.append((prev, j))
    prev = j + 1

print(f"\nstable segments (len > 20): {len(segs)}")
for (a, b) in segs[:10]:
    seg = frames[a:b]
    m = seg.mean(axis=0)
    t_std = seg.std(axis=0).mean()
    print(f"  [{a:>4}..{b:>4})  len={b-a:>3}  mean={m.mean():>8.1f}  T={(m.mean()-10000)/100:>6.2f}°C  temporal_noise={t_std:>6.2f} count")

# NETD-оценка по самому длинному сегменту
if segs:
    a, b = max(segs, key=lambda s: s[1] - s[0])
    seg = frames[a:b]
    per_pix_std = seg.std(axis=0)
    print(f"\nNETD estimate (longest stable seg {a}..{b}, {b-a} frames):")
    print(f"  temporal noise, mean over pixels:   {per_pix_std.mean():.2f} count")
    print(f"  temporal noise, median:             {np.median(per_pix_std):.2f} count")
    print(f"  temporal noise, 90th percentile:    {np.percentile(per_pix_std, 90):.2f} count")
    print(f"  noise в °C (div=100):               {per_pix_std.mean()/100:.3f} °C")
