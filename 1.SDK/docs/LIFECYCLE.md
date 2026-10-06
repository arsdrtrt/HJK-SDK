# Жизненный цикл и обработка ошибок

Документ описывает типовой порядок работы с камерой через `libircam`,
точки отказа на каждом шаге, коды ошибок SDK и рекомендуемые реакции.

## 1. Типовой жизненный цикл

```
┌──────────────────────────────────────────────────────────────┐
│  1. Загрузка библиотек      dlopen(libusb, libuvc) + HIK     │
│           ↓                                                  │
│  2. USBSDK_Init              инициализация SDK               │
│           ↓                                                  │
│  3. USBSDK_EnumDevice        поиск камер                     │
│           ↓                                                  │
│  4. USBSDK_LoginDevice       логин, получение uid            │
│           ↓                                                  │
│  5. USBSDK_Get_SysInfo       модель, FW, HW, SN              │
│           ↓                                                  │
│  6. USBSDK_CreateCallBack    подписка на кадры               │
│           ↓                                                  │
│  ┌────────────────────────────────────────────┐              │
│  │  7. Рабочий цикл (пока работает приложение)│              │
│  │     • приём кадров в callback              │              │
│  │     • рендеринг                            │              │
│  │     • управление параметрами               │              │
│  │     • измерения                            │              │
│  └────────────────────────────────────────────┘              │
│           ↓                                                  │
│  8. USBSDK_Logout            освобождение uid                │
│           ↓                                                  │
│  9. Выгрузка библиотек       автоматически при exit          │
└──────────────────────────────────────────────────────────────┘
```

Порядок **обязателен**: `Init → Enum → Login → Get_SysInfo → CreateCallBack`.
Нарушение приводит к `uid = -1` или крэшу внутри SDK.

### 1.1. В нашем коде это выглядит так

```cpp
ircam::Camera cam;

// Шаги 1–6 одной операцией
if (!cam.open(0)) {
    fprintf(stderr, "open failed\n");
    return 1;           // ошибка → выходим
}

printf("Model: %s\n", cam.model().c_str());
printf("FW:    %s\n", cam.firmware().c_str());
printf("SN:    %s\n", cam.serial().c_str());

// Шаг 6: подписка на кадры
cam.set_callback([](const ircam::Frame &f) {
    // Шаг 7: обработка кадра в отдельном потоке SDK
    // ...
});

// ... основной цикл приложения ...

// Шаг 8: при выходе
cam.close();   // внутри: USBSDK_Logout + освобождение ресурсов
```

## 2. Обработка ошибок по шагам

### Шаг 1. Загрузка библиотек

SDK вызывает `dlopen()` для `libusb-1.0.so` и `libuvc.so` **без суффикса версии**
и ищет их **в текущей рабочей директории** (`cwd`).

| Симптом | Причина | Решение |
|---|---|---|
| `init libusb failed` | нет симлинка `libusb-1.0.so` в `cwd` | `ln -sf /usr/lib/x86_64-linux-gnu/libusb-1.0.so.0 libusb-1.0.so` |
| `init libuvc failed` | нет симлинка `libuvc.so` в `cwd` | `ln -sf ../x86_64/lib/libuvc.so libuvc.so` |
| `error while loading shared libraries: libHJKUSBSDK.so` | нет `LD_LIBRARY_PATH` | `export LD_LIBRARY_PATH=$HOME/sdkreverse/sdk_libs` |

Диагностика: `strace -f -e trace=openat ./apps/demo 2>&1 | grep -i "libusb\|libuvc"`

### Шаг 2. `USBSDK_Init()`

Возвращает `1` при успехе, `0` или отрицательное при ошибке.

```cpp
int r = USBSDK_Init();
if (r != 1) {
    fprintf(stderr, "USBSDK_Init failed: %s\n", USBSDK_GetLastError());
    return 1;
}
```

Возможные причины отказа:
- нет прав на USB (запуск от непривилегированного пользователя без udev);
- `libusb`/`libuvc` не загрузились (см. шаг 1);
- несовместимая версия ядра / USB-стека.

### Шаг 3. `USBSDK_EnumDevice()`

Возвращает:
- `0` — камера найдена;
- `-2` — **init libusb failed** (не путать с «камера не найдена»);
- `-1` или другое отрицательное — камера не найдена / другая ошибка.

| Код | Причина | Решение |
|---|---|---|
| `0` | ok | — |
| `-2` | libusb не инициализировался | см. шаг 1, вероятно, нет симлинка `libusb-1.0.so` |
| `-1` | камера не подключена / занята | `lsusb -d 2bdf:0102`, `sudo modprobe -r uvcvideo` |

```cpp
USB_Camera_Info cam_info;
memset(&cam_info, 0, sizeof(cam_info));
cam_info.dwSize = sizeof(cam_info);

int r = USBSDK_EnumDevice(&cam_info);
if (r != 0) {
    fprintf(stderr, "EnumDevice ret=%d err=%s\n", r, USBSDK_GetLastError());
    if (r == -2) {
        fprintf(stderr, "  hint: нужен симлинк libusb-1.0.so в текущей директории\n");
    }
    return 1;
}
```

### Шаг 4. `USBSDK_LoginDevice()`

Возвращает **uid** (неотрицательное число) или **отрицательное** при ошибке.

Возможные причины отказа:
- устройство перехвачено другим драйвером (`uvcvideo`, `usbhid`);
- камера уже залогинена в другом процессе;
- недостаточно прав (запуск без sudo без udev-правила).

```cpp
int uid = USBSDK_LoginDevice(&cam_info);
if (uid < 0) {
    fprintf(stderr, "Login failed: %s\n", USBSDK_GetLastError());
    return 1;
}
```

### Шаг 5. `USBSDK_Get_SysInfo()`

Не критичен: если не сработал — модель/FW/SN просто не покажутся, работа продолжается.

```cpp
USB_SYSTEM_INFO si;
memset(&si, 0, sizeof(si));
if (USBSDK_Get_SysInfo(uid, &si) == 0) {
    printf("FW=%s HW=%s SN=%s\n",
           si.byFirmwareVersion, si.byHardwareVersion, si.bySerialNumber);
} else {
    fprintf(stderr, "warning: Get_SysInfo failed\n");
    // продолжаем
}
```

### Шаг 6. `USBSDK_CreateCallBack()`

С этого момента SDK создаёт **свой поток**, из которого будет вызывать callback.

| Код | Значение |
|---|---|
| `0` | ok |
| иное | ошибка подписки — обычно нет активного логина |

```cpp
int r = USBSDK_CreateCallBack(uid, frame_callback, nullptr);
if (r != 0) {
    fprintf(stderr, "CreateCallBack failed: %s\n", USBSDK_GetLastError());
    USBSDK_Logout(uid);
    return 1;
}
```

### Шаг 7. Рабочий цикл

Типичные проблемы — не ошибки SDK, а гонки данных:

- **Callback приходит из чужого потока.** Любая запись в общие структуры
  должна быть под мьютексом.
- **Heap corruption при ресайзе векторов.** Если рендер и callback обращаются
  к одному буферу без синхронизации — segfault. Фикс: один общий `std::mutex`.
- **Зависание callback.** Если SDK перестал присылать кадры, приложение
  должно это заметить (watchdog, см. §4).

### Шаг 8. `USBSDK_Logout()`

Освобождает сессию, останавливает callback-поток.

```cpp
USBSDK_Logout(uid);
```

Вызывать **даже после ошибки** на предыдущих шагах — иначе SDK может держать
интерфейс USB залоченным (до переподключения кабеля).

## 3. Апгрейд прошивки: конечный автомат

Операция необратимая. Обработка ошибок критична.

### 3.1. Состояния

Из `UPGRADE_STATE` в `usbsdk.h`:

| Код | Имя | Значение | Действие |
|---|---|---|---|
| `-1` | `GET_STATE_FAILED` | не удалось прочитать состояние | повторить опрос, при повторной ошибке — прервать |
| `1` | `UPGRADE_FAILED` | апгрейд провалился | вывести ошибку, не выдёргивать USB, перезапустить камеру |
| `2` | `UPGRADE_SUCCESS` | успех | подождать 5–10 сек, выдернуть и вставить USB |
| `3` | `UPGRADE_TRANS` | идёт передача данных | продолжать ждать, не трогать USB |
| `4` | `UPGRADE_TYPE_UNMATCH` | файл не от этой модели | прервать, повторить с правильным файлом |
| `5` | `UNKNOWN` | неизвестное состояние | подождать, при повторе — прервать |

### 3.2. Цикл опроса

```cpp
if (!cam.upgrade_start("/path/to/firmware.bin")) {
    fprintf(stderr, "upgrade_start failed\n");
    return 1;
}

int last = -100;
time_t t0 = time(nullptr);

while (true) {
    int s = cam.upgrade_state();
    int elapsed = time(nullptr) - t0;

    if (s != last) {
        printf("[%3ds] state: %d\n", elapsed, s);
        last = s;
    }

    if (s == 1 || s == 2 || s == 4) break;   // конечные состояния
    if (elapsed > 300) {                     // 5 минут — таймаут
        fprintf(stderr, "timeout\n");
        break;
    }
    sleep(1);
}

if (last == 2) {
    printf("SUCCESS — переподключи USB через 5–10 секунд\n");
} else {
    printf("FAILED state=%d\n", last);
}
```

### 3.3. Что происходит внутри (по реверсу HCUSBSDK)

```
USBSDK_Upgrade(uid, path)
  └─ CUsbCommandUpgrade::Excute()
       ├─ GetUpgradePermission    → устройство даёт добро
       ├─ SetUpgradeFileSize      → HPR_FileStat + HPR_OpenFile
       ├─ SetUpgradeCrc           → CalCRC32 всего файла
       ├─ SendUpgradeDataWithRecv → чанки, ждёт подтверждения каждого
       ├─ GetDeviceUpgradeState   → парсит XML: <upgrading>, <percent>, <upgradeStatus>
       └─ CloseUpgradeHandle
```

Особенности:
- **Расширение файла не проверяется.** SDK называет его «firmware bin file path».
- **Прогресс в % недоступен** через `USBSDK_Get_Upgrade_State` — только `UPGRADE_TRANS` (без числа).
- **Отката нет.** Единственная защита — правильный файл.
- **BOOT-режим.** Во время апгрейда устройство переподключается с другим VID:PID
  и SDK находит его **по серийнику**. Поэтому:
  - не выдёргивать USB,
  - не удивляться, если в `lsusb` мелькнёт другое устройство,
  - не запускать второй экземпляр.

### 3.4. Что делать после `UPGRADE_FAILED`

1. **Не выдёргивать USB сразу** — устройство может быть в BOOT-режиме.
2. Подождать 10–30 секунд.
3. Проверить `lsusb`: устройство всё ещё видно?
4. Перезапустить `demo`:
   - если открылось и `SN` тот же — удача, повторить с правильным файлом;
   - если `EnumDevice ret=-1` — камера в BOOT-режиме, ждать до 1 минуты;
   - если `EnumDevice ret=-2` — проблема с библиотеками, не с камерой.

## 4. Watchdog: что делать при зависании

Если callback перестал вызываться (камера отвалилась, USB-сбой), приложение
замирает на неопределённое время. Нужен таймаут.

### 4.1. Счётчик кадров

```cpp
static std::atomic<uint64_t> g_last_frame_time{0};

void on_frame(const Frame &f) {
    g_last_frame_time.store(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count()
    );
    // ... обработка
}

// в главном цикле:
uint64_t last = g_last_frame_time.load();
uint64_t now  = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now().time_since_epoch()).count();

if (last != 0 && now - last > 2000) {   // 2 секунды без кадров
    fprintf(stderr, "no frames for 2s — camera disconnected?\n");
    break;
}
```

### 4.2. Что делать при зависании

1. `USBSDK_Logout(uid)` — попытка освободить сессию.
2. Выдернуть и вставить USB камеры.
3. Перезапустить приложение.
4. Если зависает снова — `sudo dmesg | tail -30`, искать ошибки USB.

## 5. Типовые сбои на практике

| Симптом | Диагноз | Действие |
|---|---|---|
| `init libusb failed` | нет симлинка в cwd | создать симлинк |
| `init libuvc failed` | нет симлинка в cwd | создать симлинк |
| `EnumDevice ret=-2` | libusb не поднялся | симлинк + `LD_LIBRARY_PATH` |
| `EnumDevice ret=-1` | камера не найдена | `lsusb`, переподключить, `modprobe -r uvcvideo` |
| `Login failed` | камера занята / нет прав | udev-правило или `sudo` |
| `camera open failed` | `open()` в `Camera` вернул false | проверить `lsusb -d 2bdf:0102`, права |
| `Segmentation fault` после `SN:` | Qt из conda + системный Qt | `conda deactivate` |
| `Segmentation fault` на Wayland | Qt5 Wayland-плагин | `QT_QPA_PLATFORM=xcb` |
| `Segmentation fault` под root | нет `XDG_RUNTIME_DIR` | `export XDG_RUNTIME_DIR=/tmp/runtime-root` |
| `undefined reference to cv::...` | OpenCV-либы до объектников | см. порядок сборки в README |
| `libssl.so.3: OPENSSL_3.2.0 not found` | старый libssl из SDK | удалить `libssl*.so*` из `sdk_libs/` |
| `attempt to claim already-claimed interface` | `uvcvideo` держит интерфейс | не критично; при сбое `modprobe -r uvcvideo` |

## 6. Минимальный шаблон правильной обработки

```cpp
int main() {
    ircam::Camera cam;

    // Шаг 1–6 с проверкой
    if (!cam.open(0)) {
        fprintf(stderr, "camera open failed\n");
        return 1;
    }

    printf("Model=%s FW=%s SN=%s\n",
           cam.model().c_str(),
           cam.firmware().c_str(),
           cam.serial().c_str());

    cam.set_callback([](const ircam::Frame &f) {
        // ...
    });

    // Шаг 7
    // ... рабочий цикл ...

    // Шаг 8 — вызывать всегда, даже при ошибке
    cam.close();
    return 0;
}
```

## 7. Что НЕ делать

- ❌ **Не выдёргивать USB во время апгрейда.** Кирпич.
- ❌ **Не логиниться дважды** из двух процессов одновременно. Второй получит
  ошибку или подвиснет.
- ❌ **Не вызывать `USBSDK_Init()` дважды** без `Cleanup`. Поведение не определено.
- ❌ **Не заливать прошивку от другой модели.** `UPGRADE_TYPE_UNMATCH` — это
  хороший исход, отсутствие проверки — плохой.
- ❌ **Не игнорировать `Logout`.** Может залипнуть USB-интерфейс.
- ❌ **Не запускать под conda.** Segfault.