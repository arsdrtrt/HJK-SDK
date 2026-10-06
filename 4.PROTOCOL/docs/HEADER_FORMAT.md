# Формат заголовков протокола ISUSB

## `_INTER_CMD_HEAD` (команда от хоста к камере)

Заполнить после реверса `libHCUSBSDK.so` (`CUsbProtocolISUSB::PackageData`).

Пока гипотеза (из символов и структуры SDK):

    struct _INTER_CMD_HEAD {
        uint32_t magic;      // ?
        uint32_t kind_id;    // ID команды (1000-4501)
        uint32_t length;     // длина payload
        uint32_t seq;        // счётчик?
        uint32_t flags;      // ?
    };

## `_INTER_USB_RET_HEAD` (ответ камеры)

    struct _INTER_USB_RET_HEAD {
        uint32_t magic;
        uint32_t kind_id;    // эхо ID
        uint32_t length;
        uint32_t status;     // 0 = OK
        // payload
    };

## Что проверить

1. Magic-числа — искать в `.rodata` `libHCUSBSDK.so`.
2. Размеры полей — из `objdump -d` на `PackageData` / `ParseData`.
3. Endianness — likely little-endian.

## Полезные команды

    # Посмотреть строки с магией
    strings ~/sdkreverse/sdk_libs/libHCUSBSDK.so | grep -iE "magic|0xdead|0xcafe|header"

    # Дизассемблировать PackageData
    objdump -d ~/sdkreverse/sdk_libs/libHCUSBSDK.so \
        | grep -A 100 "PackageData"
