#!/usr/bin/env python3
"""
stream_probe.py — перебор всех V4L2-форматов камеры.

Для каждого формата:
  - устанавливает формат через v4l2-ctl
  - снимает 5 кадров
  - считает размер кадра, min/max/mean (как uint8, uint16)
  - сохраняет первый кадр в data/streams/<name>.raw
  - формирует отчёт

Запуск:
  sudo python3 stream_probe.py /dev/video2
"""

import subprocess
import sys
import os
import re
import numpy as np

DEV = sys.argv[1] if len(sys.argv) > 1 else "/dev/video2"
OUTDIR = "data/streams"
os.makedirs(OUTDIR, exist_ok=True)


def run(cmd):
    """Выполнить команду, вернуть (returncode, stdout, stderr)."""
    p = subprocess.run(cmd, capture_output=True, text=True)
    return p.returncode, p.stdout, p.stderr


def list_formats(dev):
    """Парсим v4l2-ctl --list-formats-ext."""
    rc, out, err = run(["v4l2-ctl", "-d", dev, "--list-formats-ext"])
    if rc != 0:
        print(f"ERROR: {err}", file=sys.stderr)
        return []

    formats = []
    cur_fourcc = None

    for line in out.splitlines():
        m = re.match(r"\s*\[\d+\]:\s+'(\w+)'", line)
        if m:
            cur_fourcc = m.group(1)
            continue

        m = re.match(r"\s+Size:\s+Discrete\s+(\d+)x(\d+)", line)
        if m and cur_fourcc:
            w, h = int(m.group(1)), int(m.group(2))
            formats.append((cur_fourcc, w, h))

    return formats


def capture_one(dev, fourcc, w, h, n_frames=5):
    """Снять n_frames кадров. Возвращает (path, размер, первый кадр как bytes)."""
    out = f"/tmp/probe_{fourcc}_{w}x{h}.raw"

    cmd = [
        "v4l2-ctl", "-d", dev,
        f"--set-fmt-video=width={w},height={h},pixelformat={fourcc}",
        "--stream-mmap", f"--stream-to={out}",
        f"--stream-count={n_frames}",
    ]
    rc, stdout, stderr = run(cmd)
    if rc != 0:
        return None, None, None, stderr.strip()

    if not os.path.exists(out):
        return None, None, None, "file not created"

    size = os.path.getsize(out)
    with open(out, "rb") as f:
        head = f.read(64)

    return out, size, head, None


def analyze(path, fourcc, w, h, n_frames):
    """Анализ содержимого файла."""
    data_u8 = np.fromfile(path, dtype=np.uint8)
    n_total = data_u8.size
    frame_bytes = n_total // n_frames if n_frames else n_total

    if n_total == 0:
        return None

    info = {
        "file": path,
        "bytes": n_total,
        "frame_bytes": frame_bytes,
    }

    if fourcc in ("YUYV", "UYVY"):
        info["expected"] = w * h * 2
    elif fourcc == "MJPG":
        info["expected"] = "variable (jpeg)"
    elif fourcc == "H264":
        info["expected"] = "variable (h264)"
    else:
        info["expected"] = "?"

    # как uint8
    info["u8_min"] = int(data_u8.min())
    info["u8_max"] = int(data_u8.max())
    info["u8_mean"] = float(data_u8.mean())

    # как uint16 (little-endian)
    if n_total % 2 == 0:
        data_u16 = data_u8.view(np.uint16)
        info["u16_min"] = int(data_u16.min())
        info["u16_max"] = int(data_u16.max())
        info["u16_mean"] = float(data_u16.mean())
        info["u16_first16"] = data_u16[:16].tolist()

    # как int16 (со знаком) — для проверки на RAW14
    if n_total % 2 == 0:
        data_i16 = data_u8.view(np.int16)
        info["i16_min"] = int(data_i16.min())
        info["i16_max"] = int(data_i16.max())

    return info


def main():
    print(f"Device: {DEV}")
    print(f"Output: {OUTDIR}/")
    print()

    formats = list_formats(DEV)
    if not formats:
        print("No formats found")
        return

    print(f"Found {len(formats)} format/resolution combinations:")
    for fourcc, w, h in formats:
        print(f"  {fourcc:<6} {w:>5}x{h:<5}")
    print()

    results = []

    for fourcc, w, h in formats:
        name = f"{fourcc}_{w}x{h}"
        print(f"=== {name} ===")

        # MJPG/H264 — сжатые, особым образом
        if fourcc in ("MJPG", "H264"):
            n = 5
        else:
            n = 5

        path, size, head, err = capture_one(DEV, fourcc, w, h, n)
        if err:
            print(f"  SKIP: {err}")
            continue

        info = analyze(path, fourcc, w, h, n)
        if not info:
            print(f"  SKIP: empty file")
            continue

        # копируем первый кадр в data/
        dst = f"{OUTDIR}/{name}.raw"
        with open(path, "rb") as f:
            first = f.read(info["frame_bytes"])
        with open(dst, "wb") as f:
            f.write(first)

        print(f"  bytes total : {info['bytes']}")
        print(f"  frame bytes : {info['frame_bytes']}")
        print(f"  expected    : {info['expected']}")
        print(f"  u8  min/max/mean: {info['u8_min']:>4} {info['u8_max']:>4} {info['u8_mean']:>7.1f}")

        if "u16_min" in info:
            print(f"  u16 min/max/mean: {info['u16_min']:>6} {info['u16_max']:>6} {info['u16_mean']:>8.1f}")
            print(f"  u16 first16     : {info['u16_first16']}")

        if "i16_min" in info:
            print(f"  i16 min/max     : {info['i16_min']:>6} {info['i16_max']:>6}")

        print(f"  saved: {dst}")
        print()

        info["name"] = name
        info["fourcc"] = fourcc
        info["w"] = w
        info["h"] = h
        results.append(info)

    # --- итоговый отчёт ---
    print("=" * 70)
    print("СВОДКА")
    print("=" * 70)
    print(f"{'name':<22} {'frame_bytes':>12} {'u16 range':>20}")
    print("-" * 70)
    for r in results:
        u16_range = "—"
        if "u16_min" in r:
            span = r["u16_max"] - r["u16_min"]
            u16_range = f"{r['u16_min']}..{r['u16_max']} ({span})"
        print(f"{r['name']:<22} {r['frame_bytes']:>12} {u16_range:>20}")


if __name__ == "__main__":
    main()