# Raw → температура в libIRSDK.so

## Формула из заголовка

CALTEMP(x, y) = (x - 10000) / y

где:
    x = raw 14-битный пиксель (из Frame.buffer[i])
    y = Frame.u8TempDiv (делитель, поле 10 в заголовке кадра)
    результат в градусах Цельсия

Подтверждено дизассемблером: константа 10000.0 @ 0x65f64c
и разбор Frame2GrayDDE (0x77f46-0x77f5f).

## Заголовок кадра (32 байта перед пикселями)

struct Frame {
    u16 width;
    u16 height;
    u16 u16FpaTemp;      // температура FPA
    u16 u16EnvTemp;      // температура среды
    u8  u8TempDiv;       // делитель CALTEMP
    u8  u8DeviceType;
    u8  u8SensorType;
    u8  u8MeasureSel;    // 1 или 2 (-20..150 или 0..550 C)
    u8  u8Lens;
    u8  u8Fps;
    u8  u8TriggerFrame;
    u8  u8Reversed2;
    u32 u32FrameIndex;
    u16 u16MeasureDis;
    u8  Reversed[8];
    u8  u8Handle;
    u8  u8ObjTempFilterSw;
    // Далее идут width*height u16 пикселей
};

## Диапазоны измерения

USB_THERMOMETRY_PARAM.byTemperatureRange:
    1 → -20..150 C
    2 → 0..550 C

## Коррекция по эмиссивности, дистанции, отражёнке

IRSDK_TempPntCorrect(emiss, reflect, dis, offset, temp)
IRSDK_TempPntRevCorrect(emiss, reflect, dis, offset, temp_corr)

Дефолтные значения (из stat_global/stat_point):
    inputEmiss   = 0.98
    inputReflect = 20.0 C
    inputDis     = 2.0 м
    inputOffset  = 0

## Атмосферная трансмиссия

IRSDK_CalcAtmoTrans(f1..f8) — 8 float, скорее всего
эмиссивность/дистанция/влажность/температура воздуха.

## Открытые вопросы

1. Точная формула IRSDK_TempPntCorrect (дизасм).
2. Как получается u8TempDiv при заводской калибровке.
3. Есть ли отдельный канал Vtemp в UVC-потоке
   (в 2.RAW был замечен).

   ## Коррекция точки (IRSDK_TempPntCorrect, 0x74da0)

Формула — классическая радиометрия (Guide/FLIR-style):

    T_out = f(ε, τ, T_refl, T_atm, offset, T_raw)

Реализация через:
    - 8 float вход: emissivity, reflect_temp, distance, offset,
      T_raw, atm_temp, humidity, коррекция T
    - IRSDK_CalcAtmoTrans (атмосферное пропускание)
    - pow(x, n) — нелинейная аппроксимация закона Планка
    - clamps в диапазон [-40..100 °C] (или [−2000..2000])

Дефолты (из .rodata):
    ε        = 0.98
    T_refl   = 20.0 °C (293.15 K)
    distance = 2.0 м
    T_atm    = 20.0 °C
    offset   = 0

Клампинг:
    вход: [-1000..1000] (0x65f624 = 1000.0)
    выход: [-2000..2000] (0x65f628, 0x65f62c)

Обратная функция: IRSDK_TempPntRevCorrect (0x75200)
Внутренняя: reverse_TempPntCorrect (0x8da60)

## Атмосферное пропускание (IRSDK_CalcAtmoTrans, 0x8d760)

Стандартная модель (двойная экспонента по дистанции):

    τ(d, T) = X·exp(-d·(α + β·√ω)) + (1-X)·exp(-d·(α + γ·√ω))

где:
    d   — дистанция (м)
    ω   — влажность
    α   — 1.5 (при 20 °C)
    β   — 10.0
    γ   — 0.6
    X   — нормировочный коэффициент

α берётся из таблицы 60 float по температуре (0x660740, 240 байт).
Температура перед выбором из таблицы округляется через roundf,
клампится в [0..60] (по °C).

Диапазоны валидации:
    distance ∈ [0..1000] м
    humidity ∈ [0..1]
    T_atm    ∈ [-40..100] °C

## Что НЕ в libIRSDK.so

NUC (Non-Uniformity Correction) и FFC (Flat Field Correction) —
делаются на чипе камеры или в FPGA. В libIRSDK их нет.
На входе всегда уже скорректированный 14-бит.