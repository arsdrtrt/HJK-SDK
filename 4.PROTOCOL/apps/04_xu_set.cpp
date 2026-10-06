// 04_xu_set.cpp — отправка XU SET_CUR на Unit 10 камеры HIK/HJK
//
// Формат команды (из usbmon-анализа):
//   bmRequestType = 0x21  (Class, Interface, Host->Device)
//   bRequest      = 0x01  (SET_CUR)
//   wValue        = (selector << 8) | 0x00
//   wIndex        = (0x0A << 8) | 0x00   (Unit 10)
//   wLength       = 2
//   Data          = 2 байта
//
// Использование:
//   ./04_xu_set <selector> <hex1> <hex2>
//   ./04_xu_set 5 01 01
//
// ВАЖНО: uvcvideo НЕ отвязывается, чтобы /dev/video2 оставался живым.
// Если SET_CUR не проходит без claim_interface — раскомментируй блок CLAIM.
//
// Сборка:
//   g++ -O2 -std=c++17 -o apps/04_xu_set apps/04_xu_set.cpp \
//       $(pkg-config --cflags --libs libusb-1.0)

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <libusb-1.0/libusb.h>

#define HIK_VID  0x2bdf
#define HIK_PID  0x0102
#define XU_UNIT  10

#define SET_CUR  0x01
#define GET_CUR  0x81

#define RT_SET   0x21   // Class, Interface, Host->Device
#define RT_GET   0xa1   // Class, Interface, Device->Host

static int xu_set(libusb_device_handle *h, uint8_t sel,
                  const uint8_t *data, int len)
{
    uint16_t wValue = (uint16_t)(sel << 8) | 0x00;
    uint16_t wIndex = (uint16_t)(XU_UNIT << 8) | 0x00;
    return libusb_control_transfer(h, RT_SET, SET_CUR, wValue, wIndex,
                                   (unsigned char *)data, len, 1000);
}

static int xu_get(libusb_device_handle *h, uint8_t sel, uint8_t *buf, int len)
{
    uint16_t wValue = (uint16_t)(sel << 8) | 0x00;
    uint16_t wIndex = (uint16_t)(XU_UNIT << 8) | 0x00;
    return libusb_control_transfer(h, RT_GET, GET_CUR, wValue, wIndex,
                                   buf, len, 1000);
}

static void hexdump(const char *tag, const uint8_t *d, int n)
{
    printf("  %-7s:", tag);
    for (int i = 0; i < n; i++) printf(" %02x", d[i]);
    printf("\n");
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr,
            "Использование: %s <selector> <hex1> <hex2>\n"
            "Пример:        %s 5 01 01\n",
            argv[0], argv[0]);
        return 1;
    }

    uint8_t sel = (uint8_t)strtol(argv[1], nullptr, 0);
    uint8_t b1  = (uint8_t)strtol(argv[2], nullptr, 16);
    uint8_t b2  = (uint8_t)strtol(argv[3], nullptr, 16);
    uint8_t data[2] = { b1, b2 };

    printf("=== XU Unit %d, selector %d ===\n", XU_UNIT, sel);
    printf("SET_CUR: %02x %02x\n", b1, b2);

    libusb_context *ctx = nullptr;
    if (libusb_init(&ctx) < 0) {
        fprintf(stderr, "libusb_init failed\n");
        return 1;
    }

    libusb_device_handle *h =
        libusb_open_device_with_vid_pid(ctx, HIK_VID, HIK_PID);
    if (!h) {
        fprintf(stderr, "камера не найдена или нет прав\n");
        libusb_exit(ctx);
        return 1;
    }

    // uvcvideo НЕ отвязываем — иначе /dev/video2 пропадает после запуска.
    // XU control transfers работают и без claim_interface,
    // потому что это class-специфичные запросы к interface 0.
    //
    // Если SET_CUR вернёт ошибку (LIBUSB_ERROR_ACCESS / -1),
    // раскомментируй блок ниже:

    // ───── BEGIN CLAIM (раскомментировать при необходимости) ─────
    // #ifdef __linux__
    // if (libusb_kernel_driver_active(h, 0) == 1)
    //     libusb_detach_kernel_driver(h, 0);
    // #endif
    // int cr = libusb_claim_interface(h, 0);
    // if (cr < 0) {
    //     fprintf(stderr, "claim_interface(0) failed: %s\n",
    //             libusb_error_name(cr));
    //     libusb_close(h); libusb_exit(ctx); return 1;
    // }
    // ───── END CLAIM ─────

    // 1. Читаем текущее значение (может не работать для write-only)
    uint8_t before[16] = {0};
    int rb = xu_get(h, sel, before, 8);
    if (rb > 0) {
        hexdump("before", before, rb);
    } else {
        printf("  before:  не читается (%s)\n", libusb_error_name(rb));
    }

    // 2. Отправляем SET_CUR
    int rs = xu_set(h, sel, data, 2);
    if (rs == 2) {
        printf("  SET_CUR: ok (%d байт)\n", rs);
    } else {
        printf("  SET_CUR: ошибка (%s)\n", libusb_error_name(rs));
    }

    // 3. Читаем после
    uint8_t after[16] = {0};
    int ra = xu_get(h, sel, after, 8);
    if (ra > 0) {
        hexdump("after ", after, ra);
    }

    // ───── BEGIN RELEASE (только если был CLAIM) ─────
    // libusb_release_interface(h, 0);
    // ───── END RELEASE ─────

    libusb_close(h);
    libusb_exit(ctx);
    return 0;
}