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
- `g++` с поддержкой C++17 (GCC 11+)
- OpenCV 4.x (`pkg-config --modversion opencv4`)
- Вендорский SDK: `libHJKUSBSDK.so`, `libHCUSBSDK.so`, `libIRSDK.so`, `libhpr.so`
  (в `~/sdkreverse/x86_64/lib/`)

Установка зависимостей:

```bash
sudo apt update
sudo apt install build-essential pkg-config libopencv-dev
