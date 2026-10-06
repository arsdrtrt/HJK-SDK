#!/usr/bin/env python3
"""parse_meta.py — расшифровка UVC payload header (4 строки × 640 uint16)."""
import sys
import numpy as np
from PIL import Image

W, H, PX_H = 640, 516, 512

def load_meta(path):
    d = np.fromfile(path, dtype=np.uint16)
    n = d.size // (W * H)
    meta = np.zeros((n, 4, W), dtype=np.uint16)
    pix_mean = np.zeros(n)
    for i in range(n):
        chunk = d[i*W*H:(i+1)*W*H]
        pix_mean[i] = chunk[:PX_H*W].mean()
        meta[i] = chunk[PX_H*W:].reshape(4, W)
    return meta, pix_mean

def analyze():
    path = sys.argv[1] if len(sys.argv) > 1 else "raw_v4l2.bin"
    meta, pix_mean = load_meta(path)
    n = meta.shape[0]
    print(f"frames: {n}")
    print(f"pixel mean: first={pix_mean[0]:.1f} last={pix_mean[-1]:.1f} "
          f"std={pix_mean.std():.3f}")

    for row in range(4):
        r = meta[:, row, :]
        uniq_per_col = [len(np.unique(r[:, c])) for c in range(W)]
        const_cols = sum(1 for u in uniq_per_col if u == 1)
        change_cols = W - const_cols
        print(f"\n=== ROW {row} ===")
        print(f"  constant columns: {const_cols}/{W}")
        print(f"  changing columns: {change_cols}/{W}")

        if change_cols == 0:
            print(f"  first 16: {r[0, :16].tolist()}")
            continue

        changing = [c for c in range(W) if uniq_per_col[c] > 1]
        print(f"  changing range: {changing[0]}..{changing[-1]}")

        all_uniq = np.unique(r)
        print(f"  unique values in row: {len(all_uniq)}")
        print(f"  min/max: {all_uniq.min()} / {all_uniq.max()}")

        # top-10
        cnt = np.bincount(r.ravel().astype(np.int64))
        top10 = np.argsort(cnt)[-10:][::-1]
        print(f"  top-10 common: {[int(v) for v in top10]}")
        print(f"  their counts:  {[int(cnt[v]) for v in top10]}")

        # float16?
        try:
            f16 = r.view(np.float16)
            finite = f16[np.isfinite(f16)]
            if len(finite) > 0:
                print(f"  as float16: min={f16.min():.3e} max={f16.max():.3e} "
                      f"finite={len(finite)}/{f16.size}")
        except Exception:
            pass

        # первые 32 значения первого кадра
        print(f"  frame0 first 32: {r[0, :32].tolist()}")

    # Корреляция row2 / row3 со средним пикселей
    print(f"\n=== Корреляции ===")
    for row in [2, 3]:
        r = meta[:, row, :].astype(np.float32)
        row_mean = r.mean(axis=1)
        row_std  = r.std(axis=1)
        if row_mean.std() > 0.5:
            corr = np.corrcoef(pix_mean, row_mean)[0, 1]
            print(f"  corr(pixel_mean, row{row}_mean) = {corr:.4f}")
        print(f"  row{row}_mean: first={row_mean[0]:.1f} last={row_mean[-1]:.1f} "
              f"std={row_mean.std():.3f}")

    # Может, row 2 — это float32 через пары uint16?
    print(f"\n=== row2 as uint32 / float32 ===")
    r2 = meta[:, 2, :]
    pairs = r2.reshape(n, W // 2, 2)
    as_u32 = (pairs[:, :, 0].astype(np.uint32) |
              (pairs[:, :, 1].astype(np.uint32) << 16))
    print(f"  uint32: min={as_u32.min()} max={as_u32.max()} "
          f"unique={len(np.unique(as_u32))}")
    f32 = as_u32.view(np.float32)
    finite = f32[np.isfinite(f32)]
    if len(finite) > 0:
        print(f"  float32: min={f32.min():.3e} max={f32.max():.3e} "
              f"finite={len(finite)}/{f32.size}")

    # Визуализация row 0..3 как 2D картинки (n × 640)
    print(f"\n=== Визуализация ===")
    for row in range(4):
        r = meta[:, row, :].astype(np.float32)
        lo, hi = np.percentile(r, [1, 99])
        if hi <= lo: hi = lo + 1
        v = np.clip((r - lo) * 255 / (hi - lo), 0, 255).astype(np.uint8)
        fname = f"out_meta_row{row}.png"
        Image.fromarray(v).save(fname)
        print(f"  saved: {fname}")

if __name__ == "__main__":
    analyze()