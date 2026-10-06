# Внутренний UART-лог камеры через UVC payload header

## Открытие

V4L2-формат `YUYV 640×516` содержит не только пиксели, но и
**ASCII-строку лога** из внутреннего UART SoC камеры. Это даёт
прямой доступ к параметрам сенсора, состоянию TEC и коэффициентам
калибровки — без вендорского SDK.

## Структура кадра V4L2 640×516

```
offset 0 : 640×512 uint16 — пиксели (Vtemp отсчёты)
655 360 байт
offset 655360 : 640 uint16 — Row 0: UVC payload header
1 280 байт
offset 656640 : 640 uint16 — Row 1: расширенный header
1 280 байт
offset 657920 : 640 uint16 — Row 2: ASCII UART-лог
1 280 байт
offset 659200 : 640 uint16 — Row 3: нули (резерв)
1 280 байт
─────────────────
total: 660 480 байт на кадр
```

## Row 0 — UVC payload header

```
[2539, 1810, 0, 43, 19, 1, 0, 0, 512, 640, 164, 0, 0, 0, 0, 4, ...]
```

- `[0]` = FPA temperature (raw) — совпадает с логом `FPA is 2539`
- `[8]` = height = 512
- `[9]` = width = 640
- `[15]` = version/flag = 4
- меняются только колонки 0-1 — счётчик/PTS

## Row 1 — расширенный header

```
[52445, 43707, 198, 0, 640, 0, 512, 0, 20200, 0, 60000, 0, 0, 0, 0, 0]
```

- `52445 = 0xCCDD`, `43707 = 0xAAB9` — magic
- `640, 512` — width, height (32-bit)
- `20200` — меняется: вероятно T°C в Q2 (202.00 °C)
- `60000` — const: верхняя граница шкалы (600.00 °C)
- `198` — emissivity (0.98?) или флаг

## Row 2 — ASCII UART-лог (главное)

Кадры содержат **строку текстового лога**. Примеры уникальных строк:

```
stream type is 3!
[CAM_CTRL] The Temperature of FPA is 2539
[CAM_CTRL] The Temperature of Cavity is 36
[TEC3TempCrtl] VtempCurrent is 4810 VtempShutter is 4812
[TEC3TempCrtl] temp_x50 is 1810 temp_x100 is 14488
[TEC3TempCrtl] subsend_temp_x50 is -2 subsend_temp_x100 is -12
[TEC3TempCrtl] VtempCurrent_x100 is 38488
[task_cam] task_cam is Runing!
6T_Gray2Temp: distempCompk = 8388608,distempCompb = 0
8MM_Gray2Temp: distempCompk = 8388608,distempCompb = 0
[taskShut] TEC control enable !
OSD ERROR] []osd is timeout
```

### Расшифровка ключевых параметров

| Параметр | Диапазон | Значение |
|---|---|---|
| FPA temp | 2528..2544 | температура матрицы (raw) |
| Cavity temp | 36 (const) | температура корпуса, °C |
| VtempCurrent | 4810..4811 | опорное напряжение «сейчас» |
| VtempShutter | 4812 (const) | опорное напряжение затвора |
| temp_x50 | 1810..1811 | Cavity в Q5: 1810/50 = 36.2 °C |
| temp_x100 | 14485..14489 | Q2: 144.88 °C — требует уточнения |
| subsend_x50 | −2..−1 | поправка (−0.04 °C в Q5) |
| subsend_x100 | −15..−11 | поправка (−0.15 °C в Q2) |
| distempCompk | 2²³ = 8 388 608 | Q23 коэффициент Gray2Temp |
| distempCompb | 0 | смещение |
| VtempCurrent_x100 | 38485..38489 | питание/ток TEC |

### Ключевой вывод: пиксели = Vtemp

```
pixel_mean (V4L2) ≈ 4820
VtempCurrent = 4810..4811
VtempShutter = 4812
```

**Пиксели V4L2 — это Vtemp отсчёты болометров** (напряжения).
SDK переупаковывает их в шкалу 12000-14000 своей калибровкой.

## Периодические shutter-калибровки

Обнаружены блоки `[tec_kwkc]==tec_kwkc begin==` в логе:

| Кадр | Событие |
|---|---|
| 20 | первая калибровка |
| 185 | вторая калибровка |
| Интервал | ~165 кадров = 5.5 сек @ 30 fps |

### Структура блока `tec_kwkc`

```
[tec_kwkc] 0: AVG: 5665 T: 1730,SHUT: 1 ERR: 0 STATE: 0
[tec_kwkc] 1: AVG: 5707 T: 5041,SHUT: 2 ERR: 0 STATE: 1
[tec_kwkc] 2: AVG: 5756 T: 2621,SHUT: 2 ERR: 0 STATE: 1
[tec_kwkc] 3: AVG: 5813 T: 1638,SHUT: 2 ERR: 0 STATE: 1
[tec_kwkc] 4: AVG: 5882 T: 1149,SHUT: 2 ERR: 0 STATE: 1
[tec_kwkc] 5: AVG: 5975 T: 799, SHUT: 2 ERR: 0 STATE: 2
```

- `AVG` — накопленное среднее по кадрам (растёт: 5665 → 5975)
- `T` — текущее значение (падает: 1730 → 799)
- `SHUT` — 1 = затвор открыт, 2 = закрыт
- `STATE` — 0 = начало, 1 = выдержка, 2 = завершение

**Кадры во время калибровки — невалидны для измерений.**

## Формула Gray2Temp

Из лога:
```
distempCompk = 8388608 = 2²³
distempCompb = 0
```

Формула **фиксированной точки Q23**:
```
T_fixed = (raw_gray - distempCompb) * K / distempCompk
```
где `K` — множитель шкалы (требует уточнения).

## Практические следствия

1. **Читая V4L2 640×516, можно получать внутренние параметры** камеры
без SDK: FPA temp, состояние TEC, коэффициенты калибровки.

2. **Shutter-события видны в логе заранее** — можно пропускать
соответствующие кадры.

3. **Для точной калибровки** V4L2 → °C нужно учитывать `VtempShutter`
и `VtempCurrent`, которые SDK использует в своей формуле.

4. **Собственный клиент возможен** — если читать raw USB через libusb
без SDK, можно получать те же данные + метаданные.

## Что осталось

- [ ] Пиксельная регрессия SDK ↔ V4L2 на статичной сцене
- [ ] Определить полный вид формулы `T = f(Vtemp, FPA, Shutter)`
- [ ] Расшифровать `temp_x100` (144.88 °C) и `VtempCurrent_x100`
- [ ] Реализовать парсер лога в реальном времени
- [ ] Написать свой libusb-клиент