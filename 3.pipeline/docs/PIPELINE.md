# Pipeline обработки болометрического сигнала в libIRSDK.so

## Архитектура

libIRSDK.so — 12.9 МБ, статически вкомпилированный OpenCV 4.x
+ libjpeg + собственный thermal pipeline. Зависимости:
libstdc++, libm, libgcc_s, libc.

501 экспорт, из них ~80 — публичное API IRSDK_*, остальные
внутренние алгоритмы с external linkage (можно вызывать напрямую).

## Три поколения пайплайна

### Поколение 1: без OpenCV (старое)
Функции:
    IRSDK_Frame2GrayDDE
    IRSDK_FrameAgcDDE
    IRSDK_Frame2GrayDDE_m
    IRSDK_FrameAgcDDE_m
    IRSDK_Frame2Gray_DDE_m

Скелет Frame2GrayDDE (0x77ed0), доказано дизассемблером:

    get_rect_y16(w, h, buffer, &out, &p3, &p4, ...)
        ↓
    rude_equ(tagFrame*, u16* eq, u16& nz, u16& p3, u16& p4, int clip)
        ↓
    gf_dde(u16* src, u16* dst, u16 w, u16 h, u16* guide,
           u8* buf, u8, int, int, int, float, int, int, int,
           uint, u8, u8)
        ├── gauss_filter
        ├── guide_filter         ← DDE через guided filter
        │   ├── box_ab
        │   ├── box_meanI
        │   └── box_mean_ab
        └── plat_equ              ← основной AGC
                └── get_quant_table

### Поколение 2: OpenCV
IRSDK_Frame2Gray_DDE_m_v2 / _v3 / _v4

Скелет _v3 (0x91a30), доказано дизассемблером:

    get_frame_y16(w, h, src, dst, ...)
      ↓
    get_gray_line(tagFrame*, ushort*, uchar*, ushort, ushort)  x5
      ↓
    destripes_v_v1 + destripes_h_v1
      ↓
    cv::GuidedFilter(cv::Mat, cv::Mat, cv::Mat, int, double, float)
      ↓
    TimeFilter_gray_m(uchar*, ushort, ushort, ushort, uchar*)
      ↓
    get_quant_table_v3(uint*, ushort, ...)  или  get_quant_table
      ↓
    cv::Mat::convertTo  +  cv::ml  +  cv::pl

### Поколение 3: "gaolu"
IRSDK_Frame2Gray_DDE_m_gaolu (0x931f0)
Отличия: get_quant_table_gaolu вместо get_quant_table_v3,
         get_gray_line(ushort*) вместо (uchar*).

## Ключевой механизм потери контраста — rude_equ

rude_equ (0x69de0) — plateu equalization. Алгоритм
восстановлен по дизассемблеру полностью:

    rude_equ(tagFrame* f, u16* eq,
             u16& p1, u16& p2, u16& p3, u16& p4, int clip)
    {
        N = f->width * f->height;
        hist = calloc(65536, 4);

        // Проход 1: гистограмма
        for (i = 0; i < N; i++) {
            v = f->buffer[i];
            sum += v;
            if (v > max) max = v;
            if (v < min) min = v;
            hist[v]++;
        }

        // Проход 2: clip + кумулятивная сумма
        nz = 0;
        for (i = 1; i < 65536; i++) {
            if (hist[i] >= 1) nz++;
            hist[i] = min(hist[i], clip) + hist[i-1];
        }

        p1 = nz;
        scale = nz * 2;
        total = hist[65535];

        // Проход 3: нормализация (eq = эквализованный кадр)
        for (i = 0; i < N; i++) {
            v = f->buffer[i];
            eq[i] = hist[v] * scale / total;
        }
    }

### Что это значит для слабых целей

- clip (единственный регулятор, задаётся вызывающим) ограничивает
  вклад одного бина в кумулятивную сумму.
- Слабые цели дают мало пикселей → после clip их бин почти
  не растёт в кумулятивной сумме → наклон LUT на этих уровнях
  близок к нулю → слабая цель "утопает" в фоне.
- Чем меньше clip — тем ровнее изображение и тем хуже контраст
  слабых целей.
- scale = nz * 2, где nz — число непустых бинов. В однородной
  сцене nz мал → маленький масштаб → картинка бледная.

## plat_equ (0x98c30)

Основной AGC, вызывается из gf_dde. Принимает 8 ushort:
    6 границ сегментов (TempSeg0..5MinLimit)
    2 границы окна (TempWaveLow / TempWaveHigh)

Внутри:
    - calloc 65536*4 (256 КБ под гистограмму)
    - Выбор размера LUT по населённости гистограммы:
        256 бинов / 512 / 1024 / 2048
    - Строится piecewise LUT вызовом get_quant_table
    - Результат собирается через packuswb (u8[256])

## get_quant_table (0x6a840)

int get_quant_table(uint* out, uchar* out_u8,
                    ushort p1..p8, uchar u8Lens, float contrast)

Строит гистограмму из 8 параметров:
    out[N-1] = 0
    memset(out + p2, 0, 4*(N - p2))
    cumsum с clip по p3
    piecewise по 6 сегментам
    SIMD-растеризация в u8[256]
    финальный множитель: ceilf(value * contrast * const)

Контрольные точки: cmp $0x141 (321), cmp $0x42 (66)
Меняют режим в зависимости от количества пикселей.

## Вспомогательные таблицы (.data)

| Символ            | Размер  | Формат          | Назначение           |
|-------------------|---------|-----------------|----------------------|
| gammavalue        | 2816    | u8[11][256]     | 11 гамма-таблиц      |
| cutoffcoef        | 65536   | u16[32768]      | f(x)=8x, 14→16 scale |
| alphacoef         | 131072  | u16[65536]      | пары (a, 0x8000-a)   |
| PaletteList_RGBA  | 19456   | u8[19][256][4]  | 19 палитр            |
| PalIndex          | 256     | u8[256]         | индексы палитр       |
| globalpara        | 344     | runtime         | настройки            |

Извлечены в data/luts/.

## Константы (float, .rodata)

- 10000.0 @ 0x65f64c — база raw-пикселя в CALTEMP
- 8192.0  @ 0x660344 — множитель в guided filter DDE
- 0.0     @ 0x65f6c0 — заглушка

## Внешние параметры (строки в .so)

Из CIniFile / ISAPI:
    TempSeg0MinLimit .. TempSeg5MinLimit
    TempWaveLow, TempWaveHigh
    TempFilterCfg, TempFilterNum
    blob_temp0, blob_temp1

## Открытые вопросы

1. Как меняются p1..p8 для plat_equ в адаптивном режиме
   (в старое ветке они берутся из rude_equ, но конкретные
   формулы связывания пока не восстановлены).
2. Где читаются TempSeg* — какой файл, какой ISAPI-параметр,
   какой XU-селектор.
3. Полное восстановление clamp-констант clip в разных ветках.
4. Связь с USB_CAMERA_PARAM.dwInterFrameNoiseReduceLevel
   и dwLSEDetailLevel.

   ## Доказанный механизм потери контраста (обновление)

### Старая ветка Frame2GrayDDE

Границы AGC (p3, p4) жёстко заданы:

    p3 = p4 = (float)u8TempDiv * 0.0 + 10000.0 = 10000

Константа 0x65f6c0 (0.0) — множитель при u8TempDiv, обнуляет его влияние.
Константа 0x65f64c (10000.0) — база raw-пикселя.

Через CALTEMP это ровно 0 °C. То есть старая ветка AGC центрирована
на 0 °C независимо от сцены. Всё, что далеко от 0 °C, давится
клиппингом в rude_equ.

### Новая ветка (OpenCV, _v3/_v4)

Параметры f32MinT/f32MaxT передаются извне. Если f32MinT == f32MaxT,
включается полностью автоматический режим (см. комментарий
в IRSDK.h к IRSDK_Frame2Gray_DDE_m_v3).

## Экспериментальное подтверждение (живой кадр)

Дамп: 2.RAW/raw16.bin, 30 кадров 640×512 y16, div=100.
Сцена: 12439..13125 raw (24.4..31.2 °C), hot pixels до 56335.

Тест: apps/01_render_test.c, 8 вариантов вызова
IRSDK_Frame2Gray_DDE_m_v3. Хэши результатов:

    m0_auto                  6b3de8ef...
    m0_win_24_26             6b3de8ef...  ← идентично m0_auto
    m1_win_24_26             1e01b0a0...
    m1_win_25_27             871d0fb6...
    m1_win_24_27             85a3a523...
    m1_win_22_28             0a4d281c...
    m1_win_20_30             ddf7e302...
    m1_win_24_26_dde100      1e01b0a0...  ← идентично m1_win_24_26

ВЫВОД: u8Method — переключатель режима окна.
    method = 0 → auto AGC, minT/maxT игнорируются
    method = 1 → manual window, minT/maxT в °C реально работают

Формула преобразования окна (дизасм 0x92490..0x924ec):
    upper_raw = maxT * u8TempDiv + 10000
    lower_raw = minT * u8TempDiv + 10000
    если upper_raw <= lower_raw → fallback в auto

При method=1 параметр u8DDEcoef игнорируется (m1_24_26 == m1_24_26_dde100).

## Итог: как управлять контрастом слабых целей

Через IRSDK_Frame2Gray_DDE_m_v3:
    - method=1 + узкое окно minT..maxT → максимальный контраст
      на выбранном диапазоне температур
    - method=0 → auto AGC, адаптация к сцене, слабые цели могут
      утонуть в клиппинге rude_equ

Это подтверждает гипотезу: потерю контраста определяет выбор окна,
а не встроенные фильтры. Управление — снаружи (SDK или свой код).