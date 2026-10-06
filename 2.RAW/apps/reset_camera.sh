#!/bin/bash
# reset_camera.sh — сбросить камеру после pkill
sudo pkill -9 -f capture_raw 2>/dev/null
sudo pkill -9 -f demo 2>/dev/null
sleep 2
sudo modprobe -r uvcvideo
sleep 2
sudo modprobe uvcvideo
sleep 3
echo "Если segfault продолжается — выдерни/вставь USB камеры"
lsusb -d 2bdf:0102
