# Endpoints камеры HIK/HJK FPGA UVC (2bdf:0102)

## Дескрипторы (снято через 01_enum.cpp)

    bus=1 port=8 speed=480 Mbps (high)
    bcdUSB=0x0200 bcdDevice=0x0409
    device class=0xef subclass=0x02 protocol=0x01  ← IAD
    MaxPacketSize0=64
    Manufacturer: HIK
    Product:      Camera
    Serial:       F10615613

    Configuration 1:
      interfaces: 2

      Interface 0 (VideoControl, 0x0E/0x01/0x00):
        EP 0x83 IN interrupt  max_packet=16  interval=8

      Interface 1 (VideoStreaming, 0x0E/0x02/0x00):
        EP 0x81 IN bulk       max_packet=512

## Идентификация

| Поле | Значение | Комментарий |
|---|---|---|
| Device class | 0xEF/0x02/0x01 | IAD — стандарт для UVC |
| Product string | Camera | |
| Serial string | **F10615613** | в SDK возвращается другой — EA6334673 |
| Speed | 480 Mbps | High Speed USB 2.0 |
| Видео | bulk 0x81 | не isoch, но стандартно UVC |

## Ключевое

- **Нет vendor-specific интерфейса** — всё через UVC
- **Команды** — UVC XU control transfers на Interface 0
- **Видео** — bulk EP 0x81 на Interface 1
- **Status** — interrupt EP 0x83 на Interface 0

## Что дальше

1. Распарсить **class-specific** дескрипторы Interface 0 (VC)
   — там будут Input Terminal, Processing Unit, **Extension Unit**
2. Перечислить XU-контролы (bmControls)
3. Увидеть, как SDK-команды (ID 1000-4501) мапятся на XU

## Полезные команды

    # Все UVC controls, которые ядро уже разобрало
    v4l2-ctl -d /dev/video2 --list-ctrls -l

    # Сырые дескрипторы
    lsusb -d 2bdf:0102 -v 2>/dev/null | head -200
