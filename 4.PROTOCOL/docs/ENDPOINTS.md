# Endpoints камеры HIK/HJK FPGA UVC (2bdf:0102)

## Дескрипторы (снято через `lsusb -v`)

    Device class    0xEF/0x02/0x01  (IAD)
    bcdUSB          0x0200          (USB 2.0)
    bcdDevice       0x0409
    Manufacturer    HIK
    Product         Camera
    Serial          F10615613
    Speed           480 Mbps

## Interface 0 — VideoControl (class=0x0E/0x01, bcdUVC 1.10)

    Input Terminal (ID=2, type=0x0201 Camera Sensor)
        ↓
    Processing Unit (ID=5)
        bmControls = 0x00000000    ← нет стандартных PU контролов
        ↓
    Extension Unit (ID=10)         ← ВСЁ ЗДЕСЬ
        GUID            a29e7641-de04-47e3-8b2b-f4341aff003b
        bNumControls    23
        bControlSize    4
        bmControls      ff ff 7f 00  (все 23 контрола включены)
        ↓
    Output Terminal (ID=3, type=0x0101 USB Streaming)

    EP 0x83 IN interrupt, max_packet=16, interval=8

## Interface 1 — VideoStreaming (class=0x0E/0x02)

    EP 0x81 IN bulk, max_packet=512
    Format 1: UNCOMPRESSED, 16 bpp
    GUID = 32595559-... = "YUY2"     ← но фактически Y16!

    7 frame descriptors:
      1.  640×1032  1320960 bytes   2 × (640×516)
      2.  640×512    655360 bytes   Y16 (SDK-совместимо)
      3.  640×516    660480 bytes   Y16 + UART-логи
      4.   80×8221  1315360 bytes   нестандарт
      5.  640×517    655496 bytes   +1 строка
      6.  332×991                    нестандарт
      7.  512×1288                   нестандарт

## Вывод

- **Камера — стандартное UVC-устройство.** Никаких vendor bulk.
- **Все команды SDK** — через XU Unit 10 (`SET_CUR`/`GET_CUR`).
- **Видео** — через bulk EP 0x81 (UVC declares YUY2, sends Y16).
- 23 контрола XU = 23 группы команд SDK.
