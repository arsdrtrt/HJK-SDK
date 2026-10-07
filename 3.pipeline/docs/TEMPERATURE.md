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