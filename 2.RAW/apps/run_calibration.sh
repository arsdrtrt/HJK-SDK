#!/bin/bash
# run_calibration.sh — снять одну сцену через SDK и V4L2 для калибровки
# ВАЖНО: во время съёмки НЕ двигай камеру и не меняй сцену.

set -u   # НЕ set -e — не падаем на первом warning

N=${1:-200}
SDK_LIB="$HOME/sdkreverse/sdk_libs"
cd "$(dirname "$0")/.."

echo "=== 0. Подготовка ==="
echo "N = $N"
echo "PWD = $(pwd)"
echo "LD_LIBRARY_PATH = $SDK_LIB"

# Проверка симлинков
[ -e libusb-1.0.so ] || ln -sf /usr/lib/x86_64-linux-gnu/libusb-1.0.so.0 libusb-1.0.so
[ -e libuvc.so ]     || ln -sf ../../x86_64/lib/libuvc.so libuvc.so

# Убить возможные процессы
sudo pkill -9 -f capture_raw 2>/dev/null
sudo pkill -9 -f demo 2>/dev/null
sleep 2

echo ""
echo "=== 1. SDK capture ($N кадров) ==="
sudo env LD_LIBRARY_PATH="$SDK_LIB" ./apps/capture_raw "$N"
RC=$?
if [ $RC -ne 0 ]; then
    echo "!! capture_raw упал с кодом $RC"
    exit 1
fi

if [ ! -f raw16.bin ]; then
    echo "!! raw16.bin не создан"
    exit 1
fi

mv raw16.bin raw_sdk.bin
echo "  → raw_sdk.bin: $(stat -c%s raw_sdk.bin) байт"

echo ""
echo "=== 2. Освободить камеру для V4L2 ==="
sudo pkill -9 -f capture_raw 2>/dev/null
sleep 5

echo ""
echo "=== 3. V4L2 capture ($N кадров, 640×516) ==="
v4l2-ctl -d /dev/video2 \
  --set-fmt-video=width=640,height=516,pixelformat=YUYV \
  --stream-mmap --stream-to=/tmp/v4l2_raw.bin --stream-count="$N" \
  > /dev/null 2>&1

if [ ! -f /tmp/v4l2_raw.bin ]; then
    echo "!! V4L2 захват не удался"
    exit 1
fi

mv /tmp/v4l2_raw.bin raw_v4l2.bin
echo "  → raw_v4l2.bin: $(stat -c%s raw_v4l2.bin) байт"

echo ""
echo "=== 4. Калибровка ==="
python3 apps/calibrate.py

echo ""
echo "=== Готово ==="
ls -la raw_sdk.bin raw_v4l2.bin calibration.npz 2>/dev/null