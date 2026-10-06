# 1.SDK — libircam для HIK/HJK FPGA UVC тепловизора

Мини-SDK и приложения для тепловизора **HIK/HJK FPGA UVC Camera**
(VID:PID `2bdf:0102`). Захват 16-бит Y16, свой пайплайн рендеринга
(destripe + TNR + PE + DDE + палитры), измерение температуры, обновление прошивки.

## Железо

| Параметр | Значение |
|---|---|
| Модель | FPGA UVC Camera |
| VID:PID | `2bdf:0102` |
| Разрешение | 640×512 |
| Формат | Y16 (16 бит, 25 fps) |
| Прошивка | APP 20022 BUILD 20251015 |
| HW | FPGA 260001 BUILD 20250728 |
| SN | EA6334673 |

## Требования

- Ubuntu / Debian (x86_64)
- `g++` с поддержкой C++17 (GCC 11+)
- OpenCV 4.x (`pkg-config --modversion opencv4`)
- Вендорский SDK в `~/sdkreverse/x86_64/lib/`:
  `libHJKUSBSDK.so`, `libHCUSBSDK.so`, `libIRSDK.so`, `libhpr.so`
- X11 или XWayland (Qt5-бэкенд OpenCV сам подбирает рабочий)

Установка зависимостей:

```bash
sudo apt update
sudo apt install build-essential pkg-config libopencv-dev
```

## Структура

```
1.SDK/
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
├── docs/
│   ├── LIFECYCLE.md        жизненный цикл и обработка ошибок
│   ├── ARCHITECTURE.md     структура модулей, потоки, данные
│   └── PROTOCOL.md         реверс HCUSBSDK: карта команд, крипто
├── build.sh                сборка библиотеки и приложений
├── run-demo.sh             обёртка запуска (conda + Qt + LD_LIBRARY_PATH)
└── README.md               этот файл
```

## Быстрый старт

### 1. Симлинки (один раз)

SDK вызывает `dlopen("libusb-1.0.so")` и `dlopen("libuvc.so")` **без суффикса
версии** и ищет их **в текущей рабочей директории** (`cwd`). Без этих симлинков
SDK падает с `init libusb failed` / `init libuvc failed`.

```bash
cd ~/sdkreverse/mysdk/1.SDK

ln -sf /usr/lib/x86_64-linux-gnu/libusb-1.0.so.0 libusb-1.0.so
ln -sf ../../x86_64/lib/libuvc.so libuvc.so
```

> **Внимание на путь `../../x86_64`:** SDK лежит в `~/sdkreverse/x86_64/`,
> а ты — в `~/sdkreverse/mysdk/1.SDK/`. Отсюда до SDK ровно два уровня вверх.

Проверка:

```bash
readlink -f libuvc.so
# ожидаем: /home/master/sdkreverse/x86_64/lib/libuvc.so

ls -la libusb-1.0.so libuvc.so
```

### 2. Права на USB (один раз)

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

### 3. Сборка

```bash
cd ~/sdkreverse/mysdk/1.SDK
./build.sh
```

Скрипт соберёт:

- `libircam.a` — статическая библиотека
- `apps/demo` — витрина (единственное приложение, для которого есть исходник)

Ожидаемый вывод:

```
== 1. libircam.a ==
   ok: 913990 bytes
== 2. apps ==
   ok: apps/demo (929640 bytes)
   skip raw
   skip raw_view
   skip view
   skip fw
== 3. проверка линковки ==
   ok: apps/demo

Готово. Запуск: ./run-demo.sh
```

> `skip raw` и т.д. — это нормально, в репозитории пока только `demo.cpp`.

Если `build.sh` падает — смотри раздел «Устранение неполадок» внизу.

### 4. Запуск

**Рекомендуется — через обёртку** `run-demo.sh`:

```bash
cd ~/sdkreverse/mysdk/1.SDK
./run-demo.sh
```

Обёртка сама делает:

- `cd` в папку со скриптом
- деактивирует conda, если она активна (иначе segfault на Qt)
- добавляет `~/sdkreverse/sdk_libs` в `LD_LIBRARY_PATH`
- приглушает отладочный спам Qt

**Вручную** — то же самое:

```bash
cd ~/sdkreverse/mysdk/1.SDK
conda deactivate                              # если активна
export LD_LIBRARY_PATH=$HOME/sdkreverse/sdk_libs
./apps/demo
```

**Без udev-правила** — с `sudo`:

```bash
sudo env LD_LIBRARY_PATH=$HOME/sdkreverse/sdk_libs ./apps/demo
```

## Приложения

### `apps/demo` — витрина

Две панели рядом:

- **Слева** — сырой Y16 после линейного растяжения по перцентилям 0.5%..99.5%
- **Справа** — полный пайплайн: destripe + TNR + PE + DDE + палитра

Курсор мыши показывает температуру в °C. Инфо-строка внизу: `fps`, режим,
палитра, DDE, контраст, FPA/Env температуры.

**Клавиши:**

| Клавиша | Действие |
|---|---|
| `q` / `Esc` | выход |
| `p` | следующая палитра (0..18) |
| `m` | режим: split → pipeline → raw → split |
| `d` / `D` | DDE +0.25 / −0.25 |
| `c` / `C` | контраст +0.1 / −0.1 |
| `r` | сброс параметров |

**Палитры (`p`):**

| `pal` | Название | Холодное | Горячее |
|---|---|---|---|
| 0 | White Hot | чёрное | белое |
| 1 | Black Hot | белое | чёрное |
| 2 | Ironbow | синее/фиолетовое | оранжевое/белое |
| 3+ | Rainbow, Jet, ... | ... | ... |

> Если ironbow (`pal=2`) целиком красный — это AGC вытянул гистограмму
> в верхний диапазон. Нажми `C` несколько раз (уменьшить контраст) или
> `D` (уменьшить DDE), картинка вернётся в норму.

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

`u8TempDiv` приходит в кадре (обычно `100`). Для более точной калибровки —
`USBSDK_Get_ThermalParam()` возвращает `dwEmissivity`, `dwDistance`,
`dwTemperatureRangeUpperLimit/LowerLimit` (в формате `(T_°C + 100) × 10`).

## Важно: conda и Qt

Если у тебя активна conda-среда (в приглашении `(base)`, `(sdk)` и т.п.),
перед запуском **обязательно** деактивируй её:

```bash
conda deactivate
./apps/demo
```

**Почему:** в conda-средах свой Qt5, который конфликтует с системным Qt,
используемым OpenCV. При запуске из `(sdk)` demo падает с `Segmentation fault`
до создания окна. В логах видно `SN: EA6334673`, но строка `Keys:` не
появляется.

**Как проверить, что дело в conda:**

```bash
ldd apps/demo | grep -i qt
```

Если в выводе пути к `miniconda3`, `anaconda3` или `$CONDA_PREFIX` — это оно.
Если пути вида `/usr/lib/x86_64-linux-gnu/libQt5*` — Qt системный, conda не
при чём.

**Что делать:**

1. `conda deactivate` — иногда нужно дважды, чтобы `CONDA_PREFIX` стал пустым.
2. Проверь: `echo "CONDA_PREFIX=$CONDA_PREFIX"` — должно быть пусто.
3. Запусти `./run-demo.sh` — обёртка делает это автоматически.

## Wayland и Qt

На системах с Wayland (Ubuntu 22.04+) Qt5 может падать при создании окна.
**По умолчанию ничего форсировать не нужно** — Qt5 сам подбирает рабочий
бэкенд.

Если всё-таки падает — попробуй:

```bash
export QT_QPA_PLATFORM=xcb       # X11 через XWayland
./apps/demo
```

или

```bash
export QT_QPA_PLATFORM=wayland
./apps/demo
```

> **Не форсируй `xcb` заранее.** На некоторых системах (включая нашу)
> принудительный `xcb` **вызывает** segfault, а без него всё работает.
> Форсируй только если без него реально падает.

## Устранение неполадок

| Ошибка | Причина | Решение |
|---|---|---|
| `init libusb failed` | нет симлинка `libusb-1.0.so` в cwd | `ln -sf /usr/lib/x86_64-linux-gnu/libusb-1.0.so.0 libusb-1.0.so` |
| `init libuvc failed` | нет/битый симлинк `libuvc.so` в cwd | `ln -sf ../../x86_64/lib/libuvc.so libuvc.so` |
| `EnumDevice ret=-2` | libusb не поднялся | симлинки + `LD_LIBRARY_PATH` |
| `EnumDevice ret=-1` | камера не найдена / занята | `lsusb -d 2bdf:0102`, `sudo modprobe -r uvcvideo` |
| `camera open failed` | камера не подключена / нет прав | проверь `lsusb`, сделай udev-правило |
| `Segmentation fault` после `SN:`, `Keys:` не печатается | Qt из conda + системный Qt OpenCV | `conda deactivate` перед запуском |
| `Segmentation fault` при старте окна | `QT_QPA_PLATFORM=xcb` на системе, где он не работает | **убрать** форсирование, оставить Qt на самоопределение |
| `Segmentation fault` под root | нет `XDG_RUNTIME_DIR` | `export XDG_RUNTIME_DIR=/tmp/runtime-root` или запускай без sudo |
| `undefined reference to cv::...` | OpenCV-либы стоят до объектников | см. порядок в `build.sh` |
| `undefined reference to main` | компиляция `demo.cpp` упала, но линкер всё равно запустился | смотри ошибки компиляции выше |
| `libssl.so.3: OPENSSL_3.2.0 not found` | старый libssl из вендорского SDK | убери `libssl*.so*`, `libcrypto*.so*` из `sdk_libs/` |
| `attempt to claim already-claimed interface` | `uvcvideo` держит интерфейс | обычно не мешает; при сбое `sudo modprobe -r uvcvideo` |

## Диагностика

**Проверить все зависимости:**

```bash
ldd apps/demo | grep "not found"
# должно быть пусто
```

**Проверить откуда Qt:**

```bash
ldd apps/demo | grep -i qt
# ждём /usr/lib/x86_64-linux-gnu/libQt5*
```

**Проверить переменные окружения:**

```bash
echo "DISPLAY=$DISPLAY"
echo "WAYLAND_DISPLAY=$WAYLAND_DISPLAY"
echo "XDG_RUNTIME_DIR=$XDG_RUNTIME_DIR"
echo "XDG_SESSION_TYPE=$XDG_SESSION_TYPE"
echo "CONDA_PREFIX=$CONDA_PREFIX"
echo "LD_LIBRARY_PATH=$LD_LIBRARY_PATH"
```

**Поймать segfault с бэктрейсом:**

```bash
gdb -batch -ex run -ex bt --args ./apps/demo 2>&1 | tail -40
```

**Трассировка dlopen:**

```bash
strace -f -e trace=openat ./apps/demo 2>&1 | grep -iE "libusb|libuvc"
```

## Документация

- [`docs/LIFECYCLE.md`](docs/LIFECYCLE.md) — жизненный цикл камеры,
  обработка ошибок на каждом шаге
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — структура модулей,
  многопоточность, поток данных от сенсора до экрана
- [`docs/PROTOCOL.md`](docs/PROTOCOL.md) — реверс вендорского SDK: карта
  команд, структуры, криптография, прошивка

## Сборка под Windows

Невозможна с текущими библиотеками — вендорский SDK поставляется только для
Linux (`ELF x86-64`). Варианты:

1. **WSL2 + WSLg** — запуск из Windows через `.bat`, окно выводится в Windows.
2. **Свой клиент на libusb** — требует реверса протокола `tagINTER_CMD_HEAD`
   (см. `docs/PROTOCOL.md`).
3. **Windows SDK от вендора** — если существует, запросить по SN.

## Лицензия

Вендорский SDK (`libHJKUSBSDK.so`, `libHCUSBSDK.so`, `libIRSDK.so`,
`libhpr.so`) является собственностью производителя. В репозиторий не входит.
Весь остальной код — свободный.