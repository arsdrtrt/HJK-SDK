# Находки

## 2026-10-06 — Enumeration без SDK

`01_enum.cpp` (libusb) показывает: камера **стандартное UVC-устройство**:

- device class **0xEF/0x02/0x01** (IAD)
- Interface 0 = **VideoControl** (0x0E/0x01)
- Interface 1 = **VideoStreaming** (0x0E/0x02)
- EP 0x83 IN interrupt — status
- EP 0x81 IN bulk max_packet=512 — видео

**Vendor-specific интерфейса нет.** Это меняет гипотезу: все команды SDK
идут через **UVC Extension Unit (XU)** control transfers, а не через
bulk `_INTER_CMD_HEAD`.

Серийник в дескрипторе — `F10615613`, а SDK возвращает `EA6334673`.
Возможно, SDK читает серийник из другого места (device info через XU).

## Что работает

- [x] Enumeration через libusb без SDK
- [x] Чтение дескрипторов
- [x] Строки (Manufacturer, Product, Serial)

## Что не работает / не проверено

- [ ] Claim interface (не нужно, если только чтение дескрипторов)
- [ ] Чтение bulk-IN
- [ ] UVC XU scan

## Открытые вопросы

1. Какие XU (Extension Unit) у Interface 0, какие GUID, какие контролы?
2. Как SDK-команды (ID 1000-4501) мапятся на XU?
3. Какой UVC-формат заявлен для Interface 1 (Y16? YUYV?)
4. Почему SDK и дескриптор дают разные серийники?
