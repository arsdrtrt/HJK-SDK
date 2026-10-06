// 03_xu_dump.cpp — корректный опрос 23 XU-контролов Unit 10
// 1. GET_LEN (2 байта) — узнать длину данных
// 2. GET_INFO (1 байт) — capabilities (GET/SET)
// 3. GET_CUR правильной длины — текущее значение
//
// Сборка:
//   g++ -O2 -std=c++17 -o apps/03_xu_dump apps/03_xu_dump.cpp \
//       $(pkg-config --cflags --libs libusb-1.0)
// Запуск:
//   sudo ./apps/03_xu_dump

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <libusb-1.0/libusb.h>

#define HIK_VID    0x2bdf
#define HIK_PID    0x0102
#define XU_UNIT    10
#define XU_NUM     23

#define GET_CUR   0x81
#define GET_MIN   0x82
#define GET_MAX   0x83
#define GET_RES   0x84
#define GET_LEN   0x85
#define GET_INFO  0x86
#define GET_DEF   0x87

#define RT_GET    0xA1   // class, interface, device->host

static uint16_t wv(uint8_t sel) { return (uint16_t)(sel << 8) | 0x00; }       // interface 0
static uint16_t wi(uint8_t unit) { return (uint16_t)(unit << 8) | 0x00; }     // endpoint 0

// Запрос GET_LEN — 2 байта (uint16)
static int xu_get_len(libusb_device_handle *h, uint8_t sel, uint16_t *out) {
    uint8_t buf[2] = {0};
    int r = libusb_control_transfer(h, RT_GET, GET_LEN, wv(sel), wi(XU_UNIT),
                                    buf, 2, 1000);
    if (r == 2) { *out = (uint16_t)(buf[0] | (buf[1] << 8)); return 2; }
    return r;
}

// Запрос GET_INFO — 1 байт
static int xu_get_info(libusb_device_handle *h, uint8_t sel, uint8_t *out) {
    uint8_t buf[1] = {0};
    int r = libusb_control_transfer(h, RT_GET, GET_INFO, wv(sel), wi(XU_UNIT),
                                    buf, 1, 1000);
    if (r == 1) { *out = buf[0]; return 1; }
    return r;
}

// Универсальный GET запрос на N байт
static int xu_get(libusb_device_handle *h, uint8_t req, uint8_t sel,
                  uint8_t *buf, int len) {
    return libusb_control_transfer(h, RT_GET, req, wv(sel), wi(XU_UNIT),
                                   buf, len, 1000);
}

static void dump_hex(const char *tag, const uint8_t *d, int n) {
    printf("    %-8s [%2d]", tag, n);
    if (n > 0) {
        printf(": ");
        for (int i = 0; i < n && i < 32; i++) printf("%02x ", d[i]);
        if (n > 32) printf("...");
    }
    printf("\n");
}

static void dump_ascii(const uint8_t *d, int n) {
    // если все байты печатные или нуль — показать как строку
    for (int i = 0; i < n; i++) {
        if (d[i] == 0) break;
        if (d[i] < 0x20 || d[i] > 0x7e) return;
    }
    printf("    ascii   : \"");
    for (int i = 0; i < n && d[i]; i++) putchar(d[i]);
    printf("\"\n");
}

int main() {
    libusb_context *ctx = nullptr;
    if (libusb_init(&ctx) < 0) { fprintf(stderr, "libusb_init failed\n"); return 1; }

    libusb_device_handle *h =
        libusb_open_device_with_vid_pid(ctx, HIK_VID, HIK_PID);
    if (!h) { fprintf(stderr, "камера не найдена\n"); return 1; }

    printf("Camera: %04x:%04x\n", HIK_VID, HIK_PID);
    printf("XU Unit %d, %d selectors\n\n", XU_UNIT, XU_NUM);

#ifdef __linux__
    if (libusb_kernel_driver_active(h, 0) == 1) {
        libusb_detach_kernel_driver(h, 0);
    }
#endif
    if (libusb_claim_interface(h, 0) < 0) {
        fprintf(stderr, "claim_interface(0) failed\n");
        libusb_close(h); libusb_exit(ctx);
        return 1;
    }

    for (int sel = 1; sel <= XU_NUM; sel++) {
        uint16_t len = 0;
        uint8_t info = 0;

        int r_len  = xu_get_len(h, sel, &len);
        int r_info = xu_get_info(h, sel, &info);

        printf("=== selector %2d ===\n", sel);

        if (r_len == 2) {
            printf("    LEN      = %u байт\n", len);
        } else {
            printf("    LEN      : ошибка (%d)\n", r_len);
        }

        if (r_info == 1) {
            // биты info: 0=GET support, 1=SET support, 2=disabled
            printf("    INFO     = 0x%02x  [GET=%d SET=%d DISABLED=%d]\n",
                   info, (info & 1), (info >> 1) & 1, (info >> 2) & 1);
        } else {
            printf("    INFO     : ошибка (%d)\n", r_info);
        }

        // GET_CUR правильной длины
        if (r_len == 2 && len > 0 && len <= 256) {
            uint8_t buf[256] = {0};
            int r = xu_get(h, GET_CUR, sel, buf, len);
            if (r == len) {
                dump_hex("CUR", buf, len);
                dump_ascii(buf, len);
            } else {
                printf("    CUR      : ошибка (%d)\n", r);
            }
        }

        // GET_DEF правильной длины
        if (r_len == 2 && len > 0 && len <= 256) {
            uint8_t buf[256] = {0};
            int r = xu_get(h, GET_DEF, sel, buf, len);
            if (r == len) {
                dump_hex("DEFAULT", buf, len);
            }
        }
        printf("\n");
    }

    libusb_release_interface(h, 0);
    libusb_close(h);
    libusb_exit(ctx);
    return 0;
}
