#!/usr/bin/env bash
# build.sh — сборка libircam и приложений из папки 1.SDK/
set -e

SL="$HOME/sdkreverse/sdk_libs"
INC_SDK="../../x86_64/include"
LIB_SDK="../../x86_64/lib"

CV_CFLAGS="$(pkg-config --cflags opencv4)"
CV_LIBS="$(pkg-config --libs opencv4)"

COMMON=(
    -g -O2 -std=c++17
    -Iinclude -I"$INC_SDK"
    $CV_CFLAGS
    -L"$SL" -Wl,-rpath,"$SL"
)

HIK_LIBS=(
    -lHJKUSBSDK -lIRSDK -lHCUSBSDK -lhpr -lz
    -lpthread -ldl -lm
)

echo "== 1. libircam.a =="
g++ "${COMMON[@]}" -c src/camera.cpp   -o src/camera.o
g++ "${COMMON[@]}" -c src/renderer.cpp -o src/renderer.o
g++ "${COMMON[@]}" -c src/measure.cpp  -o src/measure.o
ar rcs libircam.a src/camera.o src/renderer.o src/measure.o
echo "   ok: $(stat -c%s libircam.a) bytes"

echo "== 2. apps =="
for app in demo raw raw_view view fw; do
    [ -f "apps/$app.cpp" ] || { echo "   skip $app"; continue; }
    g++ "${COMMON[@]}" \
        -o "apps/$app" "apps/$app.cpp" libircam.a \
        $CV_LIBS "${HIK_LIBS[@]}"
    echo "   ok: apps/$app ($(stat -c%s apps/$app) bytes)"
done

echo "== 3. проверка линковки =="
for app in demo raw raw_view view fw; do
    [ -x "apps/$app" ] || continue
    miss=$(ldd "apps/$app" | grep "not found" || true)
    [ -n "$miss" ] && { echo "   WARN $app:"; echo "$miss"; } || echo "   ok: apps/$app"
done

echo
echo "Готово. Запуск: ./run-demo.sh"
