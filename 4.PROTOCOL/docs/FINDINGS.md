# Находки

## Сессия 2026-10-06 — дескрипторы

**Ключевые данные:**

1. **Device class 0xEF/0x02/0x01 (IAD)** — стандартный UVC.
2. **Interface 0 (VideoControl):** Input Terminal → Processing Unit
   (пустой) → **Extension Unit 10** → Output Terminal.
3. **XU GUID = `a29e7641-de04-47e3-8b2b-f4341aff003b`** — кастомный HIK.
4. **XU имеет 23 контрола**, bControlSize=4 (до 32 бит).
5. **Interface 1 (VideoStreaming):** 7 frame descriptors,
   bBitsPerPixel=16, формат YUY2 (но фактически Y16).
6. Серийник в дескрипторе `F10615613`, SDK отдаёт `EA6334673`.

## Гипотеза о протоколе

Команды `USBSDK_*` упакованы в **XU control transfers**:
- `bmRequestType = 0x21` (SET_CUR) или `0xA1` (GET_CUR)
- `bRequest = SET_CUR/GET_CUR`
- `wValue = (selector << 8) | 0x00` — selector = 1..23
- `wIndex = (10 << 8) | 0x00` — unit 10, interface 0
- `wLength = size of data`

Внутри данных XU — возможно, тот самый `tagXU_COMMAND_HEAD`
из `libHCUSBSDK.so`, но это надо проверить.

## Что проверить дальше

- [ ] `02_xu_scan.cpp` — GET_CUR/GET_INFO по всем 23 селекторам
- [ ] Сравнить с `1.SDK/docs/PROTOCOL.md` — ID 1000-4501
- [ ] Захват SDK-трафика через usbmon — увидеть SET_CUR пакеты
- [ ] Разобрать упаковку XU (заголовок + payload)

## Сессия 2026-10-06 (продолжение) — usbmon capture

Снят трафик USB при работе SDK (`capture_raw 30`):

| Endpoint | Пакетов | Тип | Что |
|---|---|---|---|
| 0x00 | 202 | control | команды OUT |
| 0x02 | 4 | ? | редко |
| 0x80 | 499 | control | ответы IN |
| **0x81** | **25410** | bulk | видео |
| **0x82** | **1128** | ? | 🆕 **неизвестный** |
| 0x83 | 288 | interrupt | status |

**Распределение по типам transfers:**
- 348 × interrupt (EP 0x83)
- 701 × control (команды SDK!)
- 26482 × bulk (видео)

## Ключевое открытие

**Endpoint 0x82 активно используется SDK, но отсутствует в
дескрипторах Interface 0 и Interface 1 altsetting 0.**

Возможно:
1. **Скрытый alternate setting** Interface 1 (VideoStreaming),
   не показанный в `01_enum` (мы читали только alt 0)
2. Второй поток (может быть второй канал видео или командный)
3. SDK переключает камеру в нестандартный режим

## Что проверить

- [ ] `lsusb -v` — все alternate settings Interface 1
- [ ] Размеры пакетов EP 0x82 — постоянные?
- [ ] Разбор control transfers EP 0x00/0x80 — реальные команды SDK

## ВАЖНАЯ КОРРЕКЦИЯ (2026-10-06 вечер)

**Предыдущий анализ содержал ошибку фильтрации.**

На одной USB-шине (Bus 1) подключено несколько устройств:
- 1.1.0 — USB-хаб
- 1.4.x — Bluetooth (Realtek/Intel)
- 1.12.x — вебкамера ноутбука
- 1.17.x — **наша HIK камера**

Фильтр по `usb.endpoint_address == 0x82` поймал **пакеты всех устройств**,
и я ошибочно приписал их камере. Реально:
- **Frame 75 (EP 0x82, 15 байт)** — это **Bluetooth HCI**, не камера
- **Frame 3 (bmRequestType 0x23)** — это **USB-хаб**, не камера

**Вывод:** все дальнейшие фильтры должны учитывать `usb.device_address`
(или `usb.addr`) камеры, а не только endpoint.

Адрес камеры требует уточнения через `tshark -Y "usb.idVendor == 0x2bdf"`.

## Что делать

1. Определить точный адрес камеры в pcap
2. Отфильтровать только трафик камеры
3. Заново проанализировать control transfers и endpoints
