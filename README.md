# libircam — демо для HIK/HJK FPGA UVC тепловизора

Мини-SDK и приложения для тепловизора **HIK/HJK FPGA UVC Camera** (VID:PID `2bdf:0102`).
Захват 16-бит Y16, свой пайплайн рендеринга (destripe + TNR + PE + DDE + палитры),
измерение температуры, обновление прошивки.

## Железо

| Параметр | Значение |
|---|---|
| Модель | FPGA UVC Camera |
| VID:PID | `2bdf:0102` |
| Разрешение | 640×512 |
| Формат | Y16 (16 бит, 25 fps) |
| Прошивка | APP 20022 BUILD 20251015 |
| HW | FPGA 260001 BUILD 20250728 |

## Требования

- Ubuntu / Debian (x86_64)
- **X11 или XWayland** — Qt5-бэкенд OpenCV нестабилен на чистом Wayland
- `g++` с поддержкой C++17 (GCC 11+)
- OpenCV 4.x (`pkg-config --modversion opencv4`)
- Вендорский SDK: `libHJKUSBSDK.so`, `libHCUSBSDK.so`, `libIRSDK.so`, `libhpr.so`
  (в `~/sdkreverse/x86_64/lib/`)

Установка зависимостей:

```bash
sudo apt update
sudo apt install build-essential pkg-config libopencv-dev
```

## Структура

```
mysdk/
├── include/ircam.hpp       публичный API
├── src/
│   ├── camera.cpp          обёртка над USBSDK_*
│   ├── renderer.cpp        пайплайн 16→8 (destripe+TNR+PE+DDE+палитры)
│   └── measure.cpp         измерения температуры
├── apps/
│   ├── demo.cpp            витрина: raw + pipeline + T° под курсором
│   ├── raw.cpp             дампер Y16 в .bin
│   ├── raw_view.cpp        живой просмотр Y16
│   ├── view.cpp            просмотр с палитрами
│   └── fw.cpp              прошивка камеры
├── build.sh                сборка всего проекта
├── run-demo.sh             обёртка запуска (conda, Qt, Wayland)
└── README.md               этот файл
```

## Быстрый старт

### 1. Клонировать

```bash
git clone git@github.com:arsdrtrt/mysdk.git
cd mysdk
```

### 2. Симлинки (один раз)

SDK вызывает `dlopen("libusb-1.0.so")` и `dlopen("libuvc.so")` **без суффикса версии** и ищет их в текущей директории:

```bash
ln -sf /usr/lib/x86_64-linux-gnu/libusb-1.0.so.0 libusb-1.0.so
ln -sf ../x86_64/lib/libuvc.so libuvc.so
```

Проверка:

```bash
ls -la libusb-1.0.so libuvc.so
# libusb-1.0.so -> /usr/lib/x86_64-linux-gnu/libusb-1.0.so.0
# libuvc.so     -> ../x86_64/lib/libuvc.so
```

### 3. Права на USB (один раз)

Чтобы запускать без `sudo`:

```bash
sudo tee /etc/udev/rules.d/99-myusb.rules >/dev/null << 'EOF'
SUBSYSTEM=="usb", ATTR{idVendor}=="2bdf", ATTR{idProduct}=="0102", MODE="0666"
EOF

sudo udevadm control --reload-rules
sudo udevadm trigger
```

**Выдерни и вставь камеру** — правило применяется при подключении.

Проверка:

```bash
lsusb -d 2bdf:0102
ls -la /dev/bus/usb/*/* | grep "$(lsusb | grep 2bdf | awk '{print $4}' | tr -d ':')"
# ожидаем crw-rw-rw-
```

### 4. Сборка

```bash
cd ~/sdkreverse/mysdk
SL=$HOME/sdkreverse/sdk_libs

# библиотека
g++ -g -O2 -std=c++17 -Iinclude -I../x86_64/include \
    -c src/camera.cpp   -o src/camera.o
g++ -g -O2 -std=c++17 -Iinclude -I../x86_64/include \
    -c src/renderer.cpp -o src/renderer.o
g++ -g -O2 -std=c++17 -Iinclude -I../x86_64/include \
    -c src/measure.cpp  -o src/measure.o
ar rcs libircam.a src/camera.o src/renderer.o src/measure.o

# приложение
g++ -g -O2 -std=c++17 \
    -Iinclude -I../x86_64/include \
    $(pkg-config --cflags opencv4) \
    -o apps/demo \
    apps/demo.cpp libircam.a \
    -L$SL -Wl,-rpath,$SL \
    -lHJKUSBSDK -lIRSDK -lHCUSBSDK -lhpr -lz \
    $(pkg-config --libs opencv4) \
    -lpthread -ldl -lm
```

> **Важен порядок:** `--cflags opencv4` идёт до объектников,
> `--libs opencv4` — после `libircam.a`. Иначе `undefined reference to cv::...`.

### 5. Запуск

**Рекомендуется — через обёртку** (сама разберётся с conda, Qt, Wayland):

```bash
cd ~/sdkreverse/mysdk
./run-demo.sh
```

Создаётся один раз:

```bash
cd ~/sdkreverse/mysdk
cat > run-demo.sh << 'EOF'
#!/bin/bash
cd "$(dirname "$0")"
if [ -n "$CONDA_PREFIX" ]; then
    echo "Note: conda активна ($CONDA_PREFIX), деактивирую"
    export PATH="$(echo "$PATH" | tr ':' '\n' | grep -v conda | paste -sd:)"
    unset CONDA_PREFIX CONDA_DEFAULT_ENV CONDA_SHLVL
fi
export LD_LIBRARY_PATH="$HOME/sdkreverse/sdk_libs:$LD_LIBRARY_PATH"
export QT_QPA_PLATFORM=xcb
export QT_LOGGING_RULES='*.debug=false;qt.qpa.*=false'
exec ./apps/demo "$@"
EOF
chmod +x run-demo.sh
```

**Вручную** — то же самое:

```bash
cd ~/sdkreverse/mysdk
conda deactivate                          # если conda активна
export LD_LIBRARY_PATH=$HOME/sdkreverse/sdk_libs
export QT_QPA_PLATFORM=xcb                # обход глюков Qt+Wayland
./apps/demo
```

Без udev-правила:

```bash
sudo env LD_LIBRARY_PATH=$HOME/sdkreverse/sdk_libs ./apps/demo
```

## Приложения

### `apps/demo` — витрина

Две панели рядом:

- **Слева** — сырой Y16 после линейного растяжения по перцентилям 0.5%..99.5%
- **Справа** — полный пайплайн: destripe + TNR + PE + DDE + палитра

Курсор мыши показывает температуру в °C. Инфо-строка внизу: fps, режим, палитра, DDE, контраст.

**Клавиши:**

| Клавиша | Действие |
|---|---|
| `q` / `Esc` | выход |
| `p` | следующая палитра (0..18) |
| `m` | режим: split → pipeline → raw → split |
| `d` / `D` | DDE +0.25 / −0.25 |
| `c` / `C` | контраст +0.1 / −0.1 |
| `r` | сброс параметров |

### `apps/raw` — дампер Y16

```bash
sudo env LD_LIBRARY_PATH=$HOME/sdkreverse/sdk_libs \
    ./apps/raw /tmp/capture.bin 100
```

Записывает 100 кадров (100 × 640 × 512 × 2 = 62.5 МБ) в `/tmp/capture.bin`.

### `apps/raw_view` — живой просмотр Y16

```bash
sudo env LD_LIBRARY_PATH=$HOME/sdkreverse/sdk_libs ./apps/raw_view
```

Окно с серым изображением, overlay: min/max raw, min/max °C, fps.

### `apps/view` — просмотр с палитрами

```bash
sudo env LD_LIBRARY_PATH=$HOME/sdkreverse/sdk_libs ./apps/view 0   # white hot
sudo env LD_LIBRARY_PATH=$HOME/sdkreverse/sdk_libs ./apps/view 2   # ironbow
```

### `apps/fw` — прошивка камеры

⚠️ **Не запускать без файла прошивки от вендора** — кирпич.

```bash
sudo env LD_LIBRARY_PATH=$HOME/sdkreverse/sdk_libs \
    ./apps/fw /path/to/firmware.bin
```

Спросит `YES` для подтверждения. Процесс 1–3 минуты, USB не выдёргивать.

## Температурная формула

```
T_°C = (raw16 - 10000) / u8TempDiv
```

Где `u8TempDiv` приходит в кадре (обычно `100`). Для более точной калибровки —
`USBSDK_Get_ThermalParam()` возвращает `dwEmissivity`, `dwDistance`,
`dwTemperatureRangeUpperLimit/LowerLimit` (в формате `(T_°C + 100) × 10`).

## Устранение неполадок

| Ошибка | Причина | Решение |
|---|---|---|
| `init libusb failed` | нет симлинка `libusb-1.0.so` в cwd | `ln -sf /usr/lib/x86_64-linux-gnu/libusb-1.0.so.0 libusb-1.0.so` |
| `init libuvc failed` | нет симлинка `libuvc.so` в cwd | `ln -sf ../x86_64/lib/libuvc.so libuvc.so` |
| `EnumDevice ret=-2` | нет прав на USB | udev-правило или `sudo` |
| `camera open failed` | камера не подключена / нет прав | проверь `lsusb -d 2bdf:0102` |
| `Segmentation fault` после `SN:`, `Keys:` не печатается | Qt из conda + системный Qt OpenCV | `conda deactivate` перед запуском |
| `Segmentation fault` при старте окна под Wayland | Qt5 Wayland-плагин | `export QT_QPA_PLATFORM=xcb` |
| `Segmentation fault` под root | `XDG_RUNTIME_DIR` не задан | `export XDG_RUNTIME_DIR=/tmp/runtime-root` |
| `undefined reference to cv::...` | OpenCV-либы до объектников | см. порядок сборки выше |
| `libssl.so.3: OPENSSL_3.2.0 not found` | старый libssl из вендорского SDK | убери `libssl*.so*`, `libcrypto*.so*` из `sdk_libs/` |
| `attempt to claim already-claimed interface` | `uvcvideo` держит интерфейс | обычно не мешает; при сбое `sudo modprobe -r uvcvideo` |

## Важно: conda и Qt

Если у тебя активна conda-среда (в приглашении `(base)`, `(sdk)` и т.п.),
перед запуском **обязательно** деактивируй её:

```bash
conda deactivate
./apps/demo
```

**Почему:** в conda-средах свой Qt5, который конфликтует с системным Qt,
используемым OpenCV. При запуске из `(sdk)` demo падает с `Segmentation fault`
до создания окна. В логах видно `SN: EA6334673`, но строка `Keys:` не появляется.

**Как проверить, что дело в conda:**

```bash
ldd apps/demo | grep -i qt
```

Если в выводе пути к `miniconda3`, `anaconda3` или `$CONDA_PREFIX` — это оно.

**Обходной путь** — запуск через `sudo` (у root нет conda в окружении):

```bash
sudo env LD_LIBRARY_PATH=$HOME/sdkreverse/sdk_libs ./apps/demo
```

### Wayland

Даже без conda, на чистом Wayland Qt5 может падать при создании окна.
Фикс — принудительный X11-бэкенд:

```bash
export QT_QPA_PLATFORM=xcb
./apps/demo
```

`run-demo.sh` делает это автоматически.

## Сборка под Windows

Невозможна с текущими библиотеками — вендорский SDK поставляется только для Linux (`ELF x86-64`).
Варианты:

1. **WSL2 + WSLg** — запуск из Windows через `.bat`, окно выводится в Windows.
2. **Свой клиент на libusb** — требует реверса протокола `tagINTER_CMD_HEAD`.
3. **Windows SDK от вендора** — если существует, запросить по SN.

## Лицензия

Вендорский SDK (`libHJKUSBSDK.so`, `libHCUSBSDK.so`, `libIRSDK.so`, `libhpr.so`)
является собственностью производителя. В репозиторий не входит.
Весь остальной код — свободный.