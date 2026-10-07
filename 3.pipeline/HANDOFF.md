# HANDOFF 3.pipeline

## Контекст

Реверс libIRSDK.so (12.9 МБ). Цель — восстановить весь пайплайн
обработки болометрического сигнала и найти, что съедает контраст
слабых целей.

Вендорский заголовок: docs/vendor/IRSDK.h (34 КБ, публичное API)
Вендорский заголовок: docs/vendor/usbsdk.h (структуры параметров)

## Что уже сделано

Полный дизасм .text сохранён в /tmp/irsdk_text.txt (1.5M строк).
Если его нет — пересобрать:
    objdump -d -j .text <lib> > /tmp/irsdk_text.txt

Восстановлены:
    rude_equ (0x69de0) — plateu equalization
    plat_equ (0x98c30) — основной AGC
    get_quant_table (0x6a840) — построение LUT
    gf_dde (0x99300) — guided filter DDE
    Frame2GrayDDE (0x77ed0) — верхний уровень старой ветки
    Frame2Gray_DDE_m_v3 (0x91a30) — OpenCV-ветка

LUT выгружены в data/luts/. Формулы и открытые вопросы —
в docs/PIPELINE.md и docs/TEMPERATURE.md.

## Открытые вопросы

1. Как в адаптивном режиме формируются 8 ushort для plat_equ
   (сейчас видно, что приходят из gf_dde через стек, но
   конкретная формула связывания с содержимым кадра не ясна).
2. Где читаются TempSeg0..5MinLimit и TempWaveLow/High.
3. Полная формула IRSDK_TempPntCorrect.
4. Как SDK управляет clip во время стрима.

## Стиль работы

- Русский, без эмодзи
- Коммиты короткие, по-русски
- Автор: arsdrtrt <arsdrtrt@gmail.com>
- Проверять каждый шаг