#!/usr/bin/env python3
"""
parse_log_extended.py — сбор всех параметров из логов камеры.

Извлекает по кадрам:
  - fpa_temp, cavity_temp, vtemp_current, vtemp_shutter
  - temp_x50, temp_x100, subsend_temp_x50, subsend_temp_x100
  - distempCompk, distempCompb
  - флаги SHUT/STATE из tec_kwkc
"""
import sys
import re
import numpy as np
from collections import defaultdict

W, H, PX_H = 640, 516, 512

PATTERNS = {
    'fpa_temp':         re.compile(r'FPA is (\d+)'),
    'cavity_temp':      re.compile(r'Cavity is (\d+)'),
    'vtemp_current':    re.compile(r'VtempCurrent is (\d+)'),
    'vtemp_shutter':    re.compile(r'VtempShutter is (\d+)'),
    'temp_x50':         re.compile(r'temp_x50 is (-?\d+)'),
    'temp_x100':        re.compile(r'temp_x100 is (-?\d+)'),
    'subsend_x50':      re.compile(r'subsend_temp_x50 is (-?\d+)'),
    'subsend_x100':     re.compile(r'subsend_temp_x100 is (-?\d+)'),
    'distemp_k':        re.compile(r'distempCompk = (\d+)'),
    'distemp_b':        re.compile(r'distempCompb = (-?\d+)'),
    'vtemp_cur_x100':   re.compile(r'VtempCurrent_x100 is (\d+)'),
}


def parse_all(path):
    d = np.fromfile(path, dtype=np.uint16)
    n = d.size // (W * H)
    print(f"frames: {n}")

    # Для каждого кадра — есть ли свой текст row 2
    per_frame = []
    for i in range(n):
        chunk = d[i*W*H:(i+1)*W*H]
        row2 = chunk[PX_H*W + 2*W : PX_H*W + 3*W]
        txt = row2.tobytes().decode('ascii', errors='replace')
        per_frame.append(txt)

    # Собираем все строки
    all_lines = []
    for i, txt in enumerate(per_frame):
        for line in re.split(r'[\r\n]+', txt):
            if line.strip():
                all_lines.append((i, line.strip()))

    print(f"всего строк (frame, line): {len(all_lines)}")

    # Ищем значения по regex
    print("\n=== Все параметры из логов ===")
    found = defaultdict(list)
    for fr, line in all_lines:
        for name, pat in PATTERNS.items():
            m = pat.search(line)
            if m:
                found[name].append((fr, int(m.group(1))))

    for name in PATTERNS:
        vals = found.get(name, [])
        if not vals:
            print(f"  {name:20}: не найдено")
            continue
        frames = [f for f, _ in vals]
        numbers = [v for _, v in vals]
        print(f"  {name:20}: {len(vals):>4} найдено, "
              f"range {min(numbers)}..{max(numbers)}, "
              f"frames {min(frames)}..{max(frames)}")

    # Ищем tec_kwkc (структурированные данные)
    print("\n=== tec_kwkc блоки ===")
    in_block = False
    blocks = []
    cur_block = []
    for fr, line in all_lines:
        if 'tec_kwkc begin' in line:
            in_block = True
            cur_block = []
            continue
        if 'tec_kwkc end' in line:
            in_block = False
            if cur_block:
                blocks.append((fr, cur_block))
            continue
        if in_block:
            m = re.search(r'(\d+): AVG: (\d+) T: (\d+),SHUT: (\d+) ERR: (\d+) STATE: (\d+)', line)
            if m:
                cur_block.append(tuple(int(x) for x in m.groups()))

    print(f"найдено блоков: {len(blocks)}")
    for fr, blk in blocks:
        print(f"\n  frame {fr}: {len(blk)} строк")
        for row in blk[:8]:
            print(f"    idx={row[0]:>2}  AVG={row[1]:>5}  T={row[2]:>5}  "
                  f"SHUT={row[3]}  ERR={row[4]}  STATE={row[5]}")

    # Сохраняем как csv
    import csv
    with open("camera_log.csv", "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["frame", "param", "value"])
        for name, vals in found.items():
            for fr, v in vals:
                w.writerow([fr, name, v])
    print(f"\nsaved: camera_log.csv")


if __name__ == "__main__":
    path = sys.argv[1] if len(sys.argv) > 1 else "/tmp/meta_500.bin"
    parse_all(path)