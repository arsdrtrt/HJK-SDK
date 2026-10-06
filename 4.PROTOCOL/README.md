# 4.PROTOCOL — свой клиент на libusb без вендорского SDK

Цель: отказаться от `libHJKUSBSDK.so` и работать с камерой через
`libusb-1.0` напрямую. Разобрать реальный USB-протокол, включая
заголовки `_INTER_CMD_HEAD` / `_INTER_USB_RET_HEAD`, XU-команды,
bulk/isoch endpoints.

## Зачем

1. **Независимость** — вендорский SDK проприетарный, обновляется
   редко, привязан к конкретной версии ядра.
2. **Понимание** — как реально работают команды, есть ли скрытые
   режимы (raw без TNR, отключение AGC).
3. **Контроль** — точный тайминг, свои буферы, свои таймауты.
4. **Кроссплатформенность** — libusb работает на Linux, Windows,
   macOS без переделки.

## Что уже известно из реверса `1.SDK/docs/PROTOCOL.md`

- Транспорт: libusb (bulk) + libuvc (UVC XU)
- Заголовок команды: `_INTER_CMD_HEAD` (ISUSB) или `tagXU_COMMAND_HEAD` (UVC)
- Ответ: `_INTER_USB_RET_HEAD`
- Video flow: bulk-IN (данные от устройства)
- Control: UVC XU (SET_CUR/GET_CUR)
- ID команд: 1000–4501, см. карту в `1.SDK/docs/PROTOCOL.md`

## План

### Этап 1. Инвентаризация (готово, когда снят pcap)

- [ ] Свежий USB-сниффер при работающей камере
- [ ] Разбор дескрипторов (VID/PID, интерфейсы, endpoints)
- [ ] Список всех URB: control, bulk IN, bulk OUT, isoch
- [ ] `docs/ENDPOINTS.md` — таблица endpoints

### Этап 2. Свой enumeration

- [ ] `01_enum.cpp` — libusb_init, find_device, dump дескрипторов
- [ ] Проверка, что камера открывается без SDK
- [ ] Захват interface 1 (vendor-specific)

### Этап 3. Разбор команд

- [ ] `02_endpoints.cpp` — какие endpoints доступны
- [ ] `03_xu_scan.cpp` — найти XU, GUID, список контролов
- [ ] `04_bulk_read.cpp` — прочитать bulk-IN
- [ ] `docs/HEADER_FORMAT.md` — восстановить `_INTER_CMD_HEAD`

### Этап 4. Минимальный клиент

- [ ] Отправить команду `USBSDK_EnumDevice`-эквивалент
- [ ] Отправить `Login`
- [ ] Запустить поток
- [ ] Прочитать Y16 кадр

### Этап 5. Сравнение

- [ ] Одновременный захват через SDK и свой клиент
- [ ] Сравнение байт-в-байт
- [ ] Отчёт `docs/FINDINGS.md`

## Требования

- libusb-1.0-dev
- libuvc-dev (для XU)
- tshark + usbmon (для сниффера)
- g++ C++17

Установка:

    sudo apt install libusb-1.0-0-dev libuvc-dev tshark

## Быстрый старт

    cd ~/sdkreverse/mysdk/4.PROTOCOL
    g++ -O2 -std=c++17 -o apps/01_enum apps/01_enum.cpp \
        $(pkg-config --cflags --libs libusb-1.0)
    sudo ./apps/01_enum

## Ссылки

- [`../1.SDK/docs/PROTOCOL.md`](../1.SDK/docs/PROTOCOL.md) — карта команд из реверса
- [`../2.RAW/docs/CAMERA_LOGS.md`](../2.RAW/docs/CAMERA_LOGS.md) — UART-логи
- [libusb API](https://libusb.sourceforge.io/api-1.0/)
