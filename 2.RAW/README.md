# 2.RAW — Сырые данные с HIK/HJK тепловизора

Исследование всех путей доступа к данным сенсора камеры HIK/HJK
FPGA UVC (VID:PID `2bdf:0102`): через вендорский SDK и через
стандартный V4L2. Оценка доступности данных до NUC, измерение NETD,
расшифровка внутреннего UART-лога камеры.

## Итоги

| № | Пункт | Статус | Результат |
|---|---|---|---|
| 1 | Заголовок в Y16-потоке | ✅ | Нет, чистый `uint16_t[640×512]` |
| 2 | Что отдаёт SDK | ✅ | Радиометрический Y16 после NUC, `T=(raw-10000)/100` |
| 3 | Типы потока | ✅ | 4 пути, см. §3 |
| 4 | Доступ до NUC | ✅ | **Недоступно** — NUC в FPGA до USB |
| 5 | NETD по Y16 | ✅ | SDK: **62 mK**, V4L2: **15.6 mK** (с TNR) |
| 6 | Стабильность захвата | ✅ | udev + pkill + переподключение USB |
| 7 | **V4L2 → °C формула** | ✅ | `T = 0.012633 × raw − 39.077` (PCC=0.968) |
| 8 | UART-логи камеры | ✅ | Расшифрованы: FPA, TEC, Vtemp, shutter |
| 9 | Периодичность shutter | ✅ | Каждые ~5.5 сек (165 кадров @ 30 fps) |

## 1. Что доступно

### 1.1. Через вендорский SDK

```c
// callback
void frame_cb(void *data, void *user) {
    Frame *f = (Frame*)data;
    // f->buffer — uint16_t[640×512]
    // f->u8TempDiv = 100
    // T_°C = (raw - 10000) / 100
}
```

- **Заголовка нет** — `Frame::buffer` сразу пиксели
- **Формула:** `T_°C = (raw − 10000) / u8TempDiv`
- **Шум (500 кадров):** temporal noise median **6.22 count = 62 mK**
- **Стабильность:** требует udev-правило + переподключение после pkill

### 1.2. Через V4L2 (640×516)

```bash
v4l2-ctl -d /dev/video2 \
  --set-fmt-video=width=640,height=516,pixelformat=YUYV \
  --stream-mmap --stream-to=frame.bin --stream-count=1
```

- **Формат кадра:** 660 480 байт = 512 строк пикселей + 4 строки метаданных
- **Пиксели:** `uint16[640×512]` — Vtemp отсчёты
- **Формула:** `T_°C = 0.012633 × raw − 39.077`
- **Шум:** temporal noise median **1.56 count = 15.6 mK** (уже прошёл TNR)
- **Метаданные:** UART-лог камеры — см. §4

### 1.3. V4L2 640×512 = 8-бит превью

- Формат YUYV: `Y0 U Y1 V` (по 4 байта)
- U = V = 0x80 (серый)
- Y = 0..255 после AGC
- **Не радиометрический**, только для просмотра

## 2. Структура кадра V4L2 640×516

```
offset 0        : 640×512 uint16  — пиксели (Vtemp)
                  655 360 байт
offset 655360   : 640 uint16      — Row 0: UVC payload header
                  1 280 байт
offset 656640   : 640 uint16      — Row 1: расширенный header
                  1 280 байт
offset 657920   : 640 uint16      — Row 2: ASCII UART-лог
                  1 280 байт
offset 659200   : 640 uint16      — Row 3: нули
                  1 280 байт
─────────────────
total: 660 480 байт на кадр
```

## 3. Формулы и калибровка

### 3.1. SDK Y16 → °C

```python
def sdk_to_temp(raw_sdk):
    return (raw_sdk - 10000.0) / 100.0
```

### 3.2. V4L2 640×516 → °C

Найдена пиксельной регрессией на статичной сцене (200 кадров):

```
SDK = 1.263331 × V4L2 + 6092.324
T_°C = 0.012633 × raw_v4l2 − 39.0768
```

Метрики:
- **PCC = 0.968** (корреляция)
- **residual std = 61.0 count = 0.61 °C**
- **Проверка на средних:** diff = −0.26 count

```python
def v4l2_to_temp(raw_v4l2):
    return 0.012633 * raw_v4l2 - 39.0768
```

Проверка:
| raw_v4l2 | T °C |
|---|---|
| 4822 | 21.83 |
| 5000 | 24.08 |
| 5500 | 30.40 |

### 3.3. Ограничения формулы

- PCC < 0.99: в SDK-серию попал shutter → baseline сдвинулся
- V4L2 temporal std = 0.96 count — поток уже прошёл TNR
- SDK temporal std = 176 count — shutter-дрейф
- **Улучшение:** снимать **по 20 кадров** (быстрее shutter-события)

## 4. UART-лог камеры (главное открытие)

Row 2 в V4L2 640×516 содержит **ASCII-лог** внутреннего SoC:

```
stream type is 3!
[CAM_CTRL] The Temperature of FPA is 2539
[CAM_CTRL] The Temperature of Cavity is 36
[TEC3TempCrtl] VtempCurrent is 4810  VtempShutter is 4812
[TEC3TempCrtl] temp_x50 is 1810 temp_x100 is 14488
[TEC3TempCrtl] subsend_temp_x50 is -2  subsend_temp_x100 is -12
[TEC3TempCrtl] VtempCurrent_x100 is 38488
6T_Gray2Temp: distempCompk = 8388608,distempCompb = 0
[taskShut] TEC control enable !
```

**Ключевые параметры:**

| Параметр | Значение | Смысл |
|---|---|---|
| FPA temp | 2528..2544 | температура матрицы |
| Cavity temp | 36 (const) | температура корпуса, °C |
| VtempCurrent | 4810 | опорное напряжение «сейчас» |
| VtempShutter | 4812 | опорное напряжение затвора |
| temp_x50 | 1810 | Cavity в Q5: 1810/50 = 36.2 °C |
| temp_x100 | 14488 | Q2: 144.88 °C |
| subsend_x100 | −15..−11 | поправка (−0.15 °C) |
| distempCompk | 2²³ = 8 388 608 | Q23 коэффициент Gray2Temp |
| distempCompb | 0 | смещение |

### 4.1. Shutter-калибровка

Каждые **165 кадров (~5.5 сек)** — блок `[tec_kwkc]==tec_kwkc begin==`:

```
[tec_kwkc] 0: AVG: 5665 T: 1730,SHUT: 1 ERR: 0 STATE: 0
[tec_kwkc] 1: AVG: 5707 T: 5041,SHUT: 2 ERR: 0 STATE: 1
...
[tec_kwkc] 5: AVG: 5975 T: 799, SHUT: 2 ERR: 0 STATE: 2
```

- **AVG** растёт: 5665 → 5975 (накопление)
- **T** падает: 1730 → 799
- **SHUT**: 1=открыт, 2=закрыт
- **STATE**: 0=начало, 1=выдержка, 2=конец

**Кадры во время калибровки — невалидны для измерений.**

## 5. NETD результаты

### 5.1. SDK Y16

| Метрика | Значение |
|---|---|
| temporal noise, median | **6.22 count = 62 mK** |
| temporal noise, mean | 21.70 count = 217 mK |
| temporal noise, p90 | 62.26 count = 623 mK |

### 5.2. V4L2 640×516

| Метрика | Значение |
|---|---|
| temporal noise, median | **1.56 count = 15.6 mK** |
| На чёрном теле | **1.08 count = 10.8 mK** |

**Почему V4L2 «лучше»:** поток уже прошёл **TNR** в FPGA.
SDK отдаёт сигнал **до TNR** — более «сырой», но шумнее.

**Для NETD сенсора** — использовать SDK.
**Для практических задач** (детект, отображение) — V4L2.

### 5.3. Сравнение с рынком

| Камера | NETD (паспорт) |
|---|---|
| FLIR Lepton 3.5 | <50 mK |
| HIKMICRO Mini2 640 | <40 mK |
| Seek Compact Pro | <70 mK |
| **HIK/HJK FPGA UVC (KM640)** | **~62 mK** (SDK) |

## 6. Структура задачи

```
2.RAW/
├── README.md                    этот файл
├── apps/
│   ├── capture_raw.cpp          захват Y16 через SDK + JSONL-логи
│   ├── capture_raw              собранный бинарник
│   ├── stream_probe.py          перебор всех V4L2 форматов
│   ├── parse_meta.py            разбор UVC payload header
│   ├── parse_log.py             базовый парсер UART-логов
│   ├── parse_log_extended.py    расширенный парсер (CSV)
│   ├── calibrate.py             регрессия SDK ↔ V4L2
│   └── run_calibration.sh       снять одну сцену в обоих режимах
├── docs/
│   ├── HEADER_ANALYSIS.md       разбор структуры Y16
│   ├── STREAM_MODES.md          все доступные типы потока
│   ├── NETD_MRTD.md             методика и результаты NETD
│   └── CAMERA_LOGS.md           расшифровка UART-логов
└── data/                        (gitignore)
    └── streams/                 образцы форматов
```

## 7. Быстрый старт

### 7.1. Зависимости

```bash
sudo apt install v4l2-ctl python3-numpy python3-pil tshark
pip install numpy pillow
```

### 7.2. Права на USB (один раз)

См. `1.SDK/docs/LIFECYCLE.md`. Кратко:

```bash
sudo tee /etc/udev/rules.d/99-hik-thermal.rules >/dev/null << 'EOF'
SUBSYSTEM=="usb", ATTR{idVendor}=="2bdf", ATTR{idProduct}=="0102", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="2bdf", ATTR{idProduct}=="0102", \
  DRIVER=="uvcvideo", RUN+="/bin/sh -c 'for i in /sys/bus/usb/drivers/uvcvideo/*:1.*; do \
    case $(readlink -f $i) in *2bdf:0102*) \
      echo -n $(basename $i) > /sys/bus/usb/drivers/uvcvideo/unbind ;; esac; done'"
EOF

sudo udevadm control --reload-rules
sudo udevadm trigger
# выдернуть/вставить USB камеры
```

### 7.3. Захват через SDK

```bash
cd ~/sdkreverse/mysdk/2.RAW

SL=$HOME/sdkreverse/sdk_libs
g++ -g -O2 -std=c++17 -I../../x86_64/include \
    -o apps/capture_raw apps/capture_raw.cpp \
    -L$SL -Wl,-rpath,$SL \
    -lHJKUSBSDK -lIRSDK -lHCUSBSDK -lhpr -lz \
    -lpthread -ldl -lm

export LD_LIBRARY_PATH=$SL
sudo env LD_LIBRARY_PATH=$SL ./apps/capture_raw 500

# анализ
python3 analyze_drift.py
```

### 7.4. Захват через V4L2

```bash
cd ~/sdkreverse/mysdk/2.RAW

# сначала убить SDK-процессы
sudo pkill -9 -f capture_raw
sleep 5

# 500 кадров 640×516
v4l2-ctl -d /dev/video2 \
  --set-fmt-video=width=640,height=516,pixelformat=YUYV \
  --stream-mmap --stream-to=/tmp/v4l2.bin --stream-count=500 \
  > /dev/null

ls -la /tmp/v4l2.bin
# ожидаем 500 × 660480 = 330 240 000

# анализ пикселей и логов
python3 apps/parse_meta.py /tmp/v4l2.bin 2>&1 | tee meta_output.txt
python3 apps/parse_log_extended.py /tmp/v4l2.bin 2>&1 | tee log_extended.txt
```

### 7.5. Калибровка

```bash
cd ~/sdkreverse/mysdk/2.RAW
./apps/run_calibration.sh 2>&1 | tee calib_output.txt
cat calib_output.txt
```

## 8. Формулы для практики

```python
import numpy as np

# === SDK ===
def sdk_raw_to_celsius(raw):
    """raw: uint16, mean ~12000"""
    return (raw - 10000.0) / 100.0

# === V4L2 640×516 ===
def v4l2_raw_to_celsius(raw):
    """raw: uint16, mean ~4820"""
    return 0.012633 * raw - 39.0768

# === V4L2 → SDK (для совместимости) ===
def v4l2_to_sdk(raw_v4l2):
    return 1.263331 * raw_v4l2 + 6092.324

# === Захват и парсинг V4L2 файла ===
def load_v4l2_frames(path, n_frames=None):
    W, H, PX_H = 640, 516, 512
    d = np.fromfile(path, dtype=np.uint16)
    n = d.size // (W * H)
    if n_frames:
        n = min(n, n_frames)
    frames = np.zeros((n, PX_H, W), dtype=np.uint16)
    for i in range(n):
        frames[i] = d[i*W*H : i*W*H + PX_H*W].reshape(PX_H, W)
    return frames
```

## 9. Практические ограничения

| Что | Ограничение |
|---|---|
| V4L2 конфликтует с SDK | одновременно нельзя, нужен pkill + sleep 5 |
| uvcvideo конфликтует с SDK | udev-правило решает |
| Segfault после pkill | переподключить USB камеры |
| Shutter каждые 5.5 сек | пропускать кадры при измерениях |
| V4L2 уже с TNR | для NETD сенсора — использовать SDK |
| До NUC — недоступно | физически не выходит из FPGA |
| temp_x100 = 144.88°C | не расшифровано до конца |

## 10. Что дальше

- [ ] Уточнить формулу V4L2 → °C на **чёрном теле**
- [ ] Проверить V4L2 640×517 (5 строк метаданных вместо 4)
- [ ] Расшифровать `temp_x100` и `VtempCurrent_x100`
- [ ] Реализовать real-time парсер UART-логов
- [ ] Свой libusb-клиент без вендорского SDK

## 11. Ссылки

- `1.SDK/` — мини-SDK + демо (обёртка над вендорским SDK)
- `docs/HEADER_ANALYSIS.md` — структура Y16
- `docs/STREAM_MODES.md` — все типы потока
- `docs/NETD_MRTD.md` — NETD методика
- `docs/CAMERA_LOGS.md` — расшифровка UART-логов
- `../1.SDK/docs/PROTOCOL.md` — реверс вендорского SDK