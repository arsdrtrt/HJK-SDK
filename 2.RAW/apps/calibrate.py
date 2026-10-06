#!/usr/bin/env python3
"""
calibrate.py — калибровка V4L2 ↔ SDK на одной сцене.

Шаги:
  1. Усреднить 200 кадров SDK → sdk_mean[y,x]
  2. Усреднить 200 кадров V4L2 → v4l2_mean[y,x]
  3. Построить линейную регрессию по пикселям:
       sdk = a * v4l2 + b
  4. Проверить остатки

Запуск: python3 apps/calibrate.py
Требует: raw_sdk.bin, raw_v4l2.bin (оба 640x512 статичной сцены)
"""

import numpy as np
import sys

W, H = 640, 512

def load_sdk(path):
    d = np.fromfile(path, dtype=np.uint16)
    n = d.size // (W * H)
    frames = d[:n*W*H].reshape(n, H, W).astype(np.float32)
    return frames

def load_v4l2(path):
    d = np.fromfile(path, dtype=np.uint16)
    n = d.size // (W * 516)  # 516 строк
    frames = np.zeros((n, H, W), dtype=np.float32)
    for i in range(n):
        chunk = d[i*W*516:(i+1)*W*516]
        frames[i] = chunk[:H*W].reshape(H, W)
    return frames

def main():
    print("Загрузка SDK...")
    sdk = load_sdk("raw_sdk.bin")
    print(f"  {sdk.shape[0]} кадров, mean={sdk.mean():.2f}, std={sdk.std():.2f}")

    print("Загрузка V4L2...")
    v4l2 = load_v4l2("raw_v4l2.bin")
    print(f"  {v4l2.shape[0]} кадров, mean={v4l2.mean():.2f}, std={v4l2.std():.2f}")

    # Усредняем по времени
    sdk_mean  = sdk.mean(axis=0)   # (512, 640)
    v4l2_mean = v4l2.mean(axis=0)

    print(f"\nSDK avg:  min={sdk_mean.min():.1f} max={sdk_mean.max():.1f} mean={sdk_mean.mean():.2f}")
    print(f"V4L2 avg: min={v4l2_mean.min():.1f} max={v4l2_mean.max():.1f} mean={v4l2_mean.mean():.2f}")

    # Проверка стабильности (mean по кадрам)
    sdk_temporal  = sdk.mean(axis=(1,2))
    v4l2_temporal = v4l2.mean(axis=(1,2))
    print(f"\nSDK  temporal: first={sdk_temporal[0]:.1f} last={sdk_temporal[-1]:.1f} "
          f"std={sdk_temporal.std():.2f}")
    print(f"V4L2 temporal: first={v4l2_temporal[0]:.1f} last={v4l2_temporal[-1]:.1f} "
          f"std={v4l2_temporal.std():.2f}")

    # Линейная регрессия по пикселям
    x = v4l2_mean.ravel()
    y = sdk_mean.ravel()
    a, b = np.polyfit(x, y, 1)
    y_pred = a * x + b
    res = y - y_pred

    print(f"\n=== Регрессия по пикселям (n={len(x)}) ===")
    print(f"SDK = {a:.6f} × V4L2 + {b:.3f}")
    print(f"residual: std={res.std():.3f}  max|res|={np.abs(res).max():.2f}  "
          f"median|res|={np.median(np.abs(res)):.3f}")
    print(f"PCC: {np.corrcoef(x, y)[0,1]:.8f}")

    # Проверка формулы на среднем
    print(f"\nПроверка: a*mean(V4L2)+b = {a*v4l2_mean.mean()+b:.2f}")
    print(f"          mean(SDK)       = {sdk_mean.mean():.2f}")
    print(f"          diff            = {sdk_mean.mean() - (a*v4l2_mean.mean()+b):.2f}")

    # Формула для T°C
    # SDK: T = (raw - 10000) / 100
    # V4L2: raw_v4l2 → SDK_raw → T
    #     T = (a*raw_v4l2 + b - 10000) / 100
    print(f"\n=== Формула для T°C из V4L2 ===")
    print(f"raw_sdk  = {a:.4f} × raw_v4l2 + {b:.1f}")
    print(f"T_°C     = (raw_sdk - 10000) / 100")
    print(f"         = ({a:.4f} × raw_v4l2 + {b:.1f} - 10000) / 100")
    print(f"         = {a/100:.6f} × raw_v4l2 + {(b-10000)/100:.4f}")

    # Сохраняем
    np.savez("calibration.npz", a=a, b=b, res_std=res.std())
    print(f"\nsaved: calibration.npz")

    # Карта остатков — для визуализации проблемных зон
    from PIL import Image
    res_map = np.abs(res).reshape(H, W)
    res_norm = np.clip(res_map * 20, 0, 255).astype(np.uint8)
    Image.fromarray(res_norm).save("out_calib_residual.png")
    print(f"saved: out_calib_residual.png")


if __name__ == "__main__":
    main()