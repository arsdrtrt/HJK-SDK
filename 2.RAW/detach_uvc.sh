#!/bin/bash
# Отвязать uvcvideo от HIK-камеры (2bdf:0102). Требует sudo.
set -e
[ "$EUID" -eq 0 ] || { echo "Запусти через sudo" >&2; exit 1; }

found=0
for i in /sys/bus/usb/drivers/uvcvideo/*:1.*; do
    [ -e "$i" ] || continue
    real=$(readlink -f "$i")
    if echo "$real" | grep -q "2bdf:0102"; then
        name=$(basename "$i")
        echo "unbind uvcvideo: $name"
        echo -n "$name" > /sys/bus/usb/drivers/uvcvideo/unbind
        found=1
    fi
done

[ $found -eq 0 ] && echo "uvcvideo не держит эту камеру" || echo "Готово"
