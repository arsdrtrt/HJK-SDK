# Архитектура libircam

Документ описывает структуру мини-SDK, потоки данных и модель многопоточности.
Читать вместе с [`LIFECYCLE.md`](LIFECYCLE.md) — там жизненный цикл и обработка ошибок.

## 1. Общая схема

```
┌─────────────────────────────────────────────────────────────────┐
│                     Приложение (apps/*.cpp)                     │
│  demo, raw, raw_view, view, fw                                  │
└───────────────┬─────────────────────────────────┬───────────────┘
                │                                 │
                │ использует                      │ использует
                ▼                                 ▼
┌───────────────────────────────┐   ┌─────────────────────────────┐
│      ircam::Camera            │   │    ircam::Renderer          │
│  • open / close               │   │  • set_params               │
│  • set_callback               │   │  • render(Frame) → gray,rgb │
│  • model / firmware / serial  │   └──────────────┬──────────────┘
│  • upgrade_start / state      │                  │
│  • set_brightness / contrast  │                  │
└───────────────┬───────────────┘                  │
                │                                  │
                │ обёртка над                      │ использует
                ▼                                  ▼
┌───────────────────────────────┐   ┌─────────────────────────────┐
│      Вендорский SDK           │   │      ircam::measure         │
│  libHJKUSBSDK.so (USBSDK_*)   │   │  • temperature(Frame,x,y)   │
│  libHCUSBSDK.so (транспорт)   │   │  • stats_rect / stats_full  │
│  libIRSDK.so (рендер-функции) │   └─────────────────────────────┘
│  libhpr.so (runtime)          │
└───────────────┬───────────────┘
                │
                │ USB
                ▼
        ┌───────────────┐
        │ HIK/HJK камера│
        │ 2bdf:0102     │
        │ Y16 640×512   │
        └───────────────┘
```

## 2. Модули

### 2.1. `Camera` (src/camera.cpp, include/ircam.hpp)

Единственный интерфейс к вендорскому SDK. Скрывает:

- инициализацию `USBSDK_Init` / `USBSDK_EnumDevice` / `USBSDK_LoginDevice`;
- подписку на callback (`USBSDK_CreateCallBack`);
- преобразование SDK-шного `Frame*` в `ircam::Frame` (свой тип);
- прошивку (`USBSDK_Upgrade` / `USBSDK_Get_Upgrade_State`);
- управление параметрами (`USBSDK_Set_Brightness`, `Set_Contrast`, `Set_ThermalParam`).

**Ключевые поля класса (PIMPL):**

```cpp
class Camera {
public:
    bool open(int idx = 0);
    void close();
    bool is_open();

    std::string model()    const;
    std::string firmware() const;
    std::string serial()   const;

    void set_callback(std::function<void(const Frame &)>);

    // параметры
    bool set_brightness(int);
    bool set_contrast(int);
    bool set_noise_reduce(int);
    bool set_flip(int);
    int  get_flip();

    // термометрия
    bool get_thermal_param(USB_THERMOMETRY_PARAM *);
    bool set_thermal_param(const USB_THERMOMETRY_PARAM *);

    // прошивка
    bool upgrade_start(const std::string &path);
    int  upgrade_state();
    bool upgrade_close();

private:
    struct Impl;
    std::unique_ptr<Impl> p;   // PIMPL: SDK-типы не текут в заголовок
};
```

**PIMPL** — ключевое решение: в `ircam.hpp` нет `#include "usbsdk.h"`,
поэтому пользователь API не зависит от вендорских структур.

### 2.2. `Frame` (include/ircam.hpp)

Внутреннее представление кадра, независимое от SDK:

```cpp
struct Frame {
    int width  = 0;
    int height = 0;
    std::vector<uint16_t> data;   // Y16, row-major

    uint32_t index = 0;           // счётчик кадров

    // температурные параметры кадра (приходят с каждым кадром)
    uint16_t fpa_temp_raw = 0;
    uint16_t env_temp_raw = 0;
    uint8_t  temp_div     = 100;  // делитель для перевода raw → °C
    uint8_t  measure_sel  = 0;

    float temp_at(int x, int y) const;   // (raw - 10000) / temp_div
};
```

`std::vector` — владеющий буфер, чтобы callback из SDK-потока мог отдать
данные главному потоку без гонок. Копия делается внутри callback.

### 2.3. `Renderer` (src/renderer.cpp)

Преобразование `Frame` (16 бит) → 8-бит `gray` и `rgb`. Не знает про SDK.

```cpp
struct RenderParams {
    float plateau_k    = 2.0f;   // AGC: сила Plateau Equalization
    float contrast     = 1.0f;
    float dde_gain     = 1.2f;
    int   dde_radius   = 4;
    bool  destripe     = true;
    float tnr_alpha    = 0.4f;   // temporal noise reduction
    float gamma        = 1.0f;
    int   palette      = 0;      // 0..18
};

class Renderer {
public:
    Renderer(int width, int height);
    void set_params(const RenderParams &);
    void render(const Frame &,
                std::vector<uint8_t> &gray,
                std::vector<uint8_t> &rgb);
};
```

**Пайплайн внутри `render()`:**

```
Y16 (uint16)
  │
  ├─ 1. destripe (v + h)          → подавление вертикальных/горизонтальных полос
  │        гауссово окно R=32, σ=16
  │
  ├─ 2. TNR (motion-adaptive)     → временной фильтр по prev_frame
  │        alpha = f(|in - prev|)
  │
  ├─ 3. AGC (Plateau Equalization) → растяжение по гистограмме
  │        plateau = mean(hist) × plateau_k
  │
  ├─ 4. base/detail split          → guided base (box mean через integral image)
  │        detail = f - base
  │
  ├─ 5. DDE                        → base + gain(detail) × detail
  │        gain = f(|detail|)
  │
  ├─ 6. gamma                      → pow(v, 1/γ)
  │
  ├─ 7. contrast / brightness      → линейная коррекция
  │
  └─ 8. палитра (LUT 256×3)        → gray → rgb
```

Порядок **не случаен**: TNR до AGC (иначе AGC «дрожит»), DDE после AGC
(иначе DDE усиливает шум), палитра последней (в конце — 8 бит).

### 2.4. `measure` (src/measure.cpp)

Простые функции без состояния:

```cpp
namespace ircam::measure {
    float  temperature(const Frame &f, int x, int y);
    Stats  stats_rect (const Frame &f, const Rect &r);
    Stats  stats_full (const Frame &f);
}

struct Stats {
    float t_min, t_max, t_mean;
    int   x_min, y_min, x_max, y_max;
    int   count;
};
```

Работает напрямую с `Frame::data`, ничего не знает ни про SDK, ни про рендер.

## 3. Модель многопоточности

Три потока:

```
┌────────────────────────┐     ┌────────────────────────┐
│   Главный поток        │     │   SDK-поток            │
│   (main)               │     │   (создан SDK)         │
│                        │     │                        │
│  cv::waitKey           │     │  USBSDK callback       │
│  cv::imshow            │     │  (кадр пришёл)         │
│  Renderer::render      │     │                        │
│  мышь                  │     │  копирует Frame        │
│                        │     │  в общий буфер         │
└──────────┬─────────────┘     └──────────┬─────────────┘
           │                              │
           │                              │
           ▼                              ▼
       ┌────────────────────────────────────┐
       │         Общее состояние            │
       │  g_lock (std::mutex)               │
       │  g_latest (Frame)                  │
       │  g_frame_count (atomic)            │
       │  g_mx, g_my, g_mtemp (atomic)      │
       └────────────────────────────────────┘
                    ▲
                    │
           ┌────────┴─────────┐
           │  X11/Wayland     │
           │  поток OpenCV    │
           │  (мышь)          │
           └──────────────────┘
```

**Правила:**

1. **Callback SDK — только запись.** Не рендерить, не рисовать, не звать OpenCV
   из SDK-потока. Только скопировать кадр под мьютексом и обновить атомики.
2. **Главный поток — только чтение.** Копирует `g_latest` под мьютексом,
   рендерит, показывает.
3. **Мьютекс один.** Разделять на несколько — быстрее по бумаге, но
   провоцирует deadlock. Один `g_lock` для всего проще и безопаснее.
4. **Атомики для скаляров.** `g_frame_count`, `g_mx`, `g_my`, `g_mtemp` — без мьютекса.

## 4. Поток данных от сенсора до экрана

```
Сенсор FPGA
  │  14 бит АЦП
  ▼
NUC/FPGA (внутри камеры)
  │  Y16 (uint16, 640×512)
  ▼
USB bulk/isoch
  │
  ▼
libusb / libuvc         ← транспорт
  │
  ▼
libHCUSBSDK.so          ← упаковка в tagINTER_CMD_HEAD
  │
  ▼
libHJKUSBSDK.so         ← USBSDK_* API
  │
  ▼
SDK-поток → callback()
  │  Frame*
  ▼
ircam::Frame (копия под мьютексом)
  │
  ▼
── главный поток ──
  │
  ├─→ stretch16to8  →  gray8 (левая панель)
  │
  └─→ Renderer::render
        ├─ destripe
        ├─ TNR
        ├─ PE (AGC)
        ├─ DDE
        ├─ gamma
        ├─ contrast
        └─ палитра → rgb8 (правая панель)
  │
  ▼
cv::Mat → cv::imshow → X11/Wayland → монитор (8 бит!)
```

**Важно:** до самого монитора доходит только 8 бит на канал. 16 бит живут
только внутри пайплайна.

## 5. Сборка

Единая команда в README:

```
libircam.a  = camera.o + renderer.o + measure.o
apps/demo   = demo.cpp + libircam.a + libHJKUSBSDK.so + libIRSDK.so + OpenCV
```

Порядок флагов критичен:
- `--cflags opencv4` — до объектников,
- объектники и `.a` — до `-l*`,
- `--libs opencv4` — после `.a`.

## 6. Расширение

**Новое приложение:** добавь `apps/foo.cpp`, используй `ircam::Camera` и
`ircam::Renderer` как в `demo.cpp`, собери по шаблону.

**Новый этап рендера:** добавь в `RenderParams` параметр, реализуй функцию
в `renderer.cpp`, вставь в пайплайн. Не забудь проверить порядок.

**Другая камера:** замени `Camera::Impl` (там весь вендорский код),
`ircam::Frame` оставь тем же. `Renderer` и `measure` не меняются.