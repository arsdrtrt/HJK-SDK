# Протокол HCUSBSDK — карта команд и структуры

Результат реверса `libHCUSBSDK.so` и `libHJKUSBSDK.so` (символы, строки, `usbsdk.h`).
Используется для понимания, что SDK делает «под капотом», и для будущего
собственного клиента на чистом `libusb` / `libuvc`.

## 1. Транспорт

SDK работает поверх двух транспортов одновременно:

| Транспорт | Класс | Что передаёт |
|---|---|---|
| **UVC** (XU control) | `CUsbProtocolUVC`, `CUsbProtocolUVCBase` | команды управления (brightness, contrast, thermal) |
| **ISUSB** (vendor bulk) | `CUsbProtocolISUSB` | видео-кадры Y16, апгрейд, большие данные |

UVC идёт через `libuvc` (isochronous + control), ISUSB — через `libusb` (bulk).
Выбор делает `ChangeConnectTypeByLink`.

## 2. Формат заголовка команды

### 2.1. UVC: `tagXU_COMMAND_HEAD`

Структура командного пакета в XU control transfer. Точные байты — в дизассемблере
`CUsbProtocolUVC::PackageData` / `ParseData` (нужно открыть `libHCUSBSDK.so`
в IDA с базой `.id0/.id1/.nam/.til`, которая лежит рядом).

Логическая структура:

```
┌──────────────────────────────────────────────────┐
│ head (tagXU_COMMAND_HEAD) │
│ ├─ magic / signature │
│ ├─ kind_id ← ID команды (см. §3) │
│ ├─ payload_len │
│ └─ flags / seq │
├──────────────────────────────────────────────────┤
│ payload (сериализация IN-типа) │
└──────────────────────────────────────────────────┘
```

Проверка: `CheckHeadValid(head, len, func_name)`.

### 2.2. ISUSB: `_INTER_CMD_HEAD`

То же для vendor bulk. Дополнительно есть `_INTER_USB_RET_HEAD` — заголовок ответа.

```
[bulk OUT] _INTER_CMD_HEAD + payload
[bulk IN ] _INTER_USB_RET_HEAD + payload ответа
```

Разбор: `CUsbProtocolISUSB::ConvertData<T>` и `ParseRecvData<T>` — по одной
инстанции шаблона на каждый тип.

## 3. Карта команд

Каждая команда — шаблон `CUsbCommandConfig<ID, IN_TYPE, OUT_TYPE>`.
Список восстановлен из mangled-символов `libHCUSBSDK.so`.

### 3.1. ACS — картридер и биометрия (1000–1031)

| ID | IN | OUT | Назначение |
|---|---|---|---|
| 1000 | tagUSB_BEEP_AND_FLICKER | void | пищалка и подсветка |
| 1001 | void | tagUSB_CARD_ISSUE_VERSION | версия выпуска карт |
| 1003 | tagUSB_CARD_PROTO | void | протокол карты (M1/CPU) |
| 1004 | tagUSB_WAIT_SECOND | tagUSB_ACTIVATE_CARD_RES | активация карты |
| 1006 | tagUSB_M1_PWD_VERIFY_INFO | void | пароль M1 |
| 1007 | tagUSB_M1_BLOCK_ADDR | tagUSB_M1_BLOCK_DATA | чтение блока |
| 1008 | tagUSB_M1_BLOCK_WRITE_DATA | void | запись блока |
| 1010 | tagUSB_M1_BLOCK_OPER | tagUSB_M1_BLOCK_OPER_RES | операция над блоком |
| 1017 | void | tagUSB_CPU_CARD_RESET_RES | сброс CPU-карты |
| 1018 | tagUSB_CPU_CARD_PACK | tagUSB_CPU_CARD_PACK | APDU CPU-карты |
| 1020 | void | tagUSB_CERTIFICATE_INFO | сертификат |
| 1023 | void | tagUSB_IC_CARD_NO | UID карты |
| 1027 | tagUSB_FINGER_PRINT_OPER_PARAM | void | операция с отпечатком |
| 1028 | tagUSB_FINGER_PRINT_COND | tagUSB_FINGER_PRINT | получить отпечаток |
| 1029 | void | tagUSB_FINGER_PRINT_CONTRAST_RESULT | сравнение |
| 1030 | tagUSB_CPU_CARD_ENCRYPT | ..._RES | шифрование CPU-картой |

### 3.2. Pass-through (1999)

| ID | IN | OUT | Назначение |
|---|---|---|---|
| 1999 | tagUSB_PT_PARAM | tagUSB_PT_PARAM | ISAPI/HTTP через USB |

Через эту команду можно гонять произвольные ISAPI-запросы к камере.

### 3.3. Прошивка (2000)

| ID | IN | OUT | Назначение |
|---|---|---|---|
| 2000 | tagINTER_SYSTEM_UPDATE_OPERATE | tagUSB_SYSTEM_UPDATE_FIRMWARE_PERMIT | разрешение |
| 2000 | tagINTER_SYSTEM_UPDATE_FILESIZE | void | размер файла |
| 2000 | tagINTER_SYSTEM_UPDATE_CRC | void | CRC32 файла |
| 2000 | tagUSB_SYSTEM_UPDATE_FIRMWARE | void | блок данных |
| 2000 | void | tagUSB_SYSTEM_UPDATE_FIRMWARE_RESULT | результат |

### 3.4. Система и термометрия (2011–2111)

| ID | IN | OUT | Назначение |
|---|---|---|---|
| 2011 | void | tagUSB_SYSTEM_DEVICE_INFO | инфо об устройстве |
| 2016/2017 | ↔ | tagUSB_SYSTEM_LOCALTIME | время |
| 2018/2019 | ↔ | tagUSB_IMAGE_BRIGHTNESS | яркость |
| 2020/2021 | ↔ | tagUSB_IMAGE_CONTRAST | контраст |
| 2030/2031 | ↔ | tagUSB_THERMOMETRY_BASIC_PARAM | базовая термометрия |
| 2032/2033 | ↔ | tagUSB_THERMOMETRY_MODE | режим |
| 2034/2035 | ↔ | tagUSB_THERMOMETRY_REGIONS | регионы |
| 2040/2041 | ↔ | tagUSB_TEMPERATURE_CORRECT | коррекция |
| 2042/2043 | ↔ | tagUSB_BLACK_BODY | чёрное тело |
| 2047 | tagUSB_ROI_MAX_TEMPERATURE_SEARCH | ..._RESULT | поиск max T в ROI |
| 2065 | void | tagUSB_SYSTEM_CAPABILITIES | capabilities |
| 2084/2085 | ↔ | tagUSB_SYSTEM_INIT | инициализация |
| 2105 | void | tagUSB_IMAGE_PALETTE_DATA | палитра |

Полный список — ~100 команд в диапазоне 2011–2111, доступен из mangled-символов.

### 3.5. Видео (3001–3999)

| ID | IN | OUT | Назначение |
|---|---|---|---|
| 3001 | void | USB_VIDEO_CAPACITY | ёмкость видео |
| 3003/3004 | ↔ | tagUSB_VIDEO_PARAM | видео-параметры |
| 3011 | tagUSB_FD_RESULT_PARAM | tagUSB_IR_FRAME | IR-кадр |
| 3015/3016 | ↔ | tagUSB_VIDEO_PROPERTY | свойства видео |
| 3999 | void | tagUSB_COMMAND_STATE | последнее состояние / ошибка |

### 3.6. UVC extended (4001–4053)

| ID | Назначение |
|---|---|
| 4002 | system encrypt status |
| 4003/4004 | indicator light |
| 4007 | image WDR |
| 4010/4011 | OSD switch / cfg |
| 4016/4017 | audio volume |
| 4037 | PTZ track mode |
| 4038–4040 | PTZ preset cfg |
| 4051 | system device capabilities |
| 4052 | SVC multiple stream |

### 3.7. VCA (4101–4120)

| ID | Назначение |
|---|---|
| 4101/4102 | VCA switch |
| 4104/4105 | face threshold |
| 4106/4107 | face attributes |
| 4108/4109 | face detect rule |
| 4110/4111 | face quality |
| 4112 | pic download |
| 4117/4118 | face detect |

### 3.8. Версия (4500–4501)

| ID | Назначение |
|---|---|
| 4501 | tagUSB_DEVICE_VERSION |

## 4. Апгрейд прошивки — последовательность

Восстановлено из строк `libHCUSBSDK.so`:

```
USBSDK_Upgrade(uid, path)
│
└─ CUsbCommandUpgrade::Excute()
│
├─ GetUpgradePermission
│ → ID 2000 IN=tagINTER_SYSTEM_UPDATE_OPERATE
│ ← OUT=tagUSB_SYSTEM_UPDATE_FIRMWARE_PERMIT
│
├─ SetUpgradeFileSize
│ → ID 2000 IN=tagINTER_SYSTEM_UPDATE_FILESIZE
│ (HPR_FileStat → HPR_OpenFile)
│
├─ SetUpgradeCrc
│ → ID 2000 IN=tagINTER_SYSTEM_UPDATE_CRC
│ (CalCRC32 всего файла, HPR_ReadFile)
│
├─ SendUpgradeDataWithRecv
│ → ID 2000 IN=tagUSB_SYSTEM_UPDATE_FIRMWARE
│ (чанки: dwMsgTotalNum, dwCurMsgIndex, dwHaveSendSize)
│ (ждёт подтверждения каждого чанка: struUpdateRes.dwExpMsgIndex)
│
├─ GetDeviceUpgradeState
│ ← парсит XML от устройства:
│ <upgrading>, <upgradeStatus>, <percent>
│
└─ CloseUpgradeHandle
```

### 4.1. BOOT-режим

Из строки `CUsbDeviceManager::AcsDeviceUpgrade`:

```
BOOT mode device - VID=[0x%X] PID=[0x%X] Serial Number=[%s]
Reopen success / Reopen failed
```

Устройство во время апгрейда **переподключается** (другой VID:PID),
SDK находит его по серийнику. Поэтому `UPGRADE_TRANS` может длиться
дольше, чем видно устройство в `lsusb`.

### 4.2. Коды состояния

Из `enum UPGRADE_STATE` (usbsdk.h):

| Код | Имя | Действие |
|---|---|---|
| -1 | GET_STATE_FAILED | повторить опрос |
| 1 | UPGRADE_FAILED | прервать, разбираться |
| 2 | UPGRADE_SUCCESS | переподключить USB через 5–10 сек |
| 3 | UPGRADE_TRANS | продолжать ждать |
| 4 | UPGRADE_TYPE_UNMATCH | файл не от этой модели |
| 5 | UNKNOWN | подождать, при повторе — прервать |

### 4.3. Что SDK НЕ делает

- **Не проверяет расширение файла.** Любое имя пойдёт.
- **Не даёт прогресс в %** через публичный API — только `UPGRADE_TRANS`.
- **Не даёт отката.** Единственная защита — правильный файл.
- **Не проверяет версию/модель до старта.** Только в процессе.

## 5. Криптография

Из `nm -D` и `strings`:

### 5.1. Симметричное

- AES — `CEncryptManager::AES_Encrypt/Decrypt`, `SSLTrans_AesCbc*`, `AesEcb*`, `AesGcm*`
- SM4 — `SSLTrans_SM4`, `EVP_SM4_CBC/ECB/CFB128/OFB/CTR`
- ChaCha20 — `SSLTrans_ChaCha20`

### 5.2. Асимметричное

- RSA — `GenerateRSAKey`, `GenerateRSAKey2048`, `DecryptByPrivateKey`
- SM2 — `SSLTrans_SM2CreateKeyPair`, `SSLTrans_SM2Encrypt/Decrypt`
- ECDH — `SSLTrans_GenerateECDHPubPriKey`, `ECDHComputeShareKey`

### 5.3. Хеши

- SHA-256/384/512 — `StrSHA256`, `SHA256Password`
- SM3 — `SSLTrans_SM3`
- PBKDF2-HMAC-SHA256 и PBKDF2-HMAC-SM3 — `SSLTrans_PKCS5_PBKDF2_HMAC_*`

### 5.4. Где в протоколе

Логин (`CUsbDeviceACS::Login`):
```
GetDeviceEncryptAndChecksum ← выбрать метод
GetDeviceAESKey / GenerateAesKey ← обмен ключом
GenerateIrreversiblePassword ← PBKDF2(пароль)
SetChallenge / SetSalt / SetIterations
```

После логина все пакеты идут зашифрованными + с checksum. Поэтому сниффер
«в лоб» показывает мусор после login.

## 6. Защиты

| Тип | Что видно |
|---|---|
| Символы не стрипятся | mangled-имена классов видны |
| Отсутствие VMProtect/Themida | секции стандартные |
| Отсутствие CFG flattening | обычные прологи/эпилоги |
| Криптография протокола | AES/SM4/RSA/ECDH после login |
| Подпись прошивки | **возможно** есть — надо проверить в IDA вызовы `RSA_verify`, `SM2_verify`, `EVP_DigestVerify*` рядом с `CUsbCommandUpgrade::Excute` |
| Проверка VID/PID | `CUsbDeviceFilter::CheckVID/CheckPID` |
| Привязка по SN | `FindExisted(..., serial, ...)` |

## 7. Что нужно, чтобы доделать протокол

Реверс **структур** невозможен по именам. Нужен дизассемблер:

1. Открой `libHCUSBSDK.so` в IDA Pro / Ghidra.
2. Подгрузи базу `.id0/.id1/.nam/.til` (лежат рядом с .so) — имена восстановятся.
3. Найди:
- `CUsbProtocolUVC::PackageData`
- `CUsbProtocolUVCBase::ParseRecvData<T>`
- `CUsbProtocolISUSB::PackageData`
- `CUsbProtocolISUSB::ParseRecvData<T>`
4. Восстанови layout `tagXU_COMMAND_HEAD` и `_INTER_CMD_HEAD` — из первых 32–64 байт
каждой функции.
5. Загляни в `.rodata` — там могут быть magic-константы, публичные ключи, GUID.

## 8. Что можно делать уже сейчас

| Задача | Как |
|---|---|
| Захват Y16 | `USBSDK_CreateCallBack` → `Frame::buffer` |
| Рендер | `IRSDK_Frame2Gray_DDE_m_v3` или свой pipeline |
| ISAPI через USB | ID 1999 (`tagUSB_PT_PARAM`) |
| Прошивка | `USBSDK_Upgrade` (нужен правильный файл) |
| Управление параметрами | ID 2018–2064 (get/set) |
| VCA | ID 4101–4120 |

## 9. Источники

- `nm -D --defined-only ~/sdkreverse/x86_64/lib/libHJKUSBSDK.so`
- `nm -D --defined-only ~/sdkreverse/x86_64/lib/libHCUSBSDK.so`
- `strings -n 6 ~/sdkreverse/x86_64/lib/libHCUSBSDK.so`
- `strings -n 6 ~/sdkreverse/x86_64/lib/libHJKUSBSDK.so`
- `grep -n "upgrade\|firmware" ~/sdkreverse/x86_64/include/usbsdk.h`
- IDA-база: `*.so.id0`, `*.so.id1`, `*.so.nam`, `*.so.til`