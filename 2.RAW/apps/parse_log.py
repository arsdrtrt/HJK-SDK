#!/usr/bin/env python3
"""
parse_log.py — извлечение debug-логов камеры из UVC payload header.

Row 2 (640 uint16 = 1280 байт) содержит ASCII-текст — внутренний
UART-лог SoC камеры.
"""
import sys
import re
import numpy as np
from collections import Counter

W, H, PX_H = 640, 516, 512

def extract_all_logs(path, rows=(0,1,2,3)):
    d = np.fromfile(path, dtype=np.uint16)
    n = d.size // (W * H)
    out = {r: b'' for r in rows}
    for i in range(n):
        chunk = d[i*W*H:(i+1)*W*H]
        for r in rows:
            row = chunk[PX_H*W + r*W : PX_H*W + (r+1)*W]
            out[r] += row.tobytes()
    return out, n

def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "/tmp/meta_500.bin"
    print(f"file: {path}")
    logs, n = extract_all_logs(path)
    print(f"frames: {n}")
    print()

    # Собираем все строки из всех 4 rows
    full = b''
    for r in range(4):
        full += logs[r]
        # разделители — 0x0D0A или одиночные
        full += b'\n'

    txt = full.decode('ascii', errors='replace')

    # Разбиваем на строки по CR/LF
    lines = re.split(r'[\r\n]+', txt)
    lines = [l.strip() for l in lines if l.strip() and len(l) > 2]

    print(f"всего строк: {len(lines)}")
    print()

    # Уникальные
    uniq = Counter(lines)
    print(f"уникальных: {len(uniq)}")
    print()

    # Топ-40 самых частых
    print("=== Топ-40 частых строк ===")
    for line, cnt in uniq.most_common(40):
        # обрезаем слишком длинные
        s = line[:120]
        print(f"  [{cnt:>4}] {s}")

    print()
    print("=== Все строки, содержащие ключевые слова ===")
    keywords = ['temp', 'Temp', 'TEMP', 'FPA', 'Shutter', 'SHUT',
                'TEC', 'tec', 'Cavity', 'Gray2Temp', 'comp',
                'kx', 'ky', 'gain', 'offset', 'nuc', 'NUC',
                'calib', 'Coeff', 'coef', 'k =', 'b =',
                'cur', 'CUR', 'volt', 'Volt']
    for line, cnt in uniq.most_common():
        if any(k in line for k in keywords):
            print(f"  [{cnt:>4}] {line[:160]}")

    print()
    print("=== Все строки с числами (парсим как key=value) ===")
    kv_re = re.compile(r'([A-Za-z_][A-Za-z0-9_]*)\s*[=:]\s*([-+]?\d+(?:\.\d+)?)')
    all_kv = []
    for line in lines:
        for m in kv_re.finditer(line):
            all_kv.append((m.group(1), m.group(2)))

    kv_uniq = Counter(k for k, v in all_kv)
    print(f"уникальных ключей: {len(kv_uniq)}")
    for key, cnt in kv_uniq.most_common(50):
        # примеры значений
        vals = [v for k, v in all_kv if k == key][:5]
        print(f"  {key:30} x{cnt}   примеры: {vals}")


if __name__ == "__main__":
    main()