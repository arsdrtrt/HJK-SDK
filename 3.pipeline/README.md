# 3.pipeline — реверс пайплайна обработки libIRSDK.so

Цель: восстановить полный тракт обработки болометрического сигнала
от 14-битного raw до 8-битного вывода и до температуры в градусах.

## Что уже сделано

- Найдена библиотека: `/home/master/sdkreverse/x86_64/lib/libIRSDK.so`
- Заголовки: `docs/vendor/IRSDK.h`, `docs/vendor/usbsdk.h`
- Дизассемблированы и восстановлены ключевые функции:
    rude_equ, plat_equ, gf_dde, get_quant_table, Frame2GrayDDE,
    Frame2Gray_DDE_m_v3, Frame2Gray_DDE_m_v4, Frame2Gray_DDE_m_gaolu
- Извлечены готовые LUT из .data: gammavalue, cutoffcoef,
    alphacoef, PaletteList_RGBA, PalIndex
- Восстановлена формула raw → температура:
    CALTEMP(x, y) = (x - 10000) / y
    где y = Frame.u8TempDiv

## Структура
3.pipeline/
├── README.md
├── HANDOFF.md
├── docs/
│ ├── PIPELINE.md полный пайплайн + механизм потери контраста
│ ├── TEMPERATURE.md raw → температура
│ └── vendor/ вендорские заголовки (для справки)
├── apps/ (пусто, для будущих утилит)
├── data/ (в .gitignore) выгрузки дизасма, LUT-ы
└── captures/ (в .gitignore) сырые дампы


## Главные результаты

1. AGC работает через plateu equalization (`rude_equ` / `plat_equ`).
   Clip в кумулятивной гистограмме определяет, как сильно давятся
   слабые цели.
2. DDE — guided filter (`gf_dde` / `cv::GuidedFilter`), усиливает
   детали поверх AGC.
3. Готовые LUT (гамма, палитры) лежат в .data и извлечены в data/luts/.
4. Формула температуры восстановлена точно.

## Дальше

- Дизасм IRSDK_TempPntCorrect / IRSDK_CalcAtmoTrans.
- Найти где физически читаются TempSeg0..5MinLimit и TempWave.
- Понять, как SDK управляет clip во время стрима.