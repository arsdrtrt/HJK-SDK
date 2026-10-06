// 02_xu_scan.cpp — перечислить 23 XU-контрола камеры через libusb
// Читает GET_CUR по каждому селектору, показывает hex.
//
// Сборка:
//   g++ -O2 -std=c++17 -o apps/02_xu_scan apps/02_xu_scan.cpp \
//       $(pkg-config --cflags --libs libusb-1.0)
// Запуск:
//   sudo ./apps/02_xu_scan

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <libusb-1.0/libusb.h>

#define HIK_VID 0x2bdf
#define HIK_PID 0x0102
#define XU_UNIT_ID 10
#define XU_NUM_CONTROLS 23
#define CTRL_SIZE 4       // bControlSize из дескриптора

// UVC XU bRequest коды
#define UVC_SET_CUR 0x01
#define UVC_GET_CUR 0x81
#define UVC_GET_MIN 0x82
#define UVC_GET_MAX 0x83
#define UVC_GET_RES 0x84
#define UVC_GET_LEN 0x85
#define UVC_GET_INFO 0x86
#define UVC_GET_DEF 0x87

// bmRequestType для XU (class, interface, host->device / device->host)
#define RT_SET  (0x21)   // class, interface, host->device
#define RT_GET  (0xa1)   // class, interface, device->host

static void hexdump(const char *tag, const uint8_t *d, int n) {
    printf("  %s:", tag);
    for (int i = 0; i < n; i++) printf(" %02x", d[i]);
    printf("\n");
}

// UVC XU control request:
// wValue = (control_selector << 8) | interface_num
// wIndex = (unit_id << 8) | endpoint_num
static int xu_get(libusb_device_handle *h, uint8_t selector,
                  uint8_t *buf, int len, const char *label)
{
    uint16_t wValue = (selector << 8) | 0x00;    // interface 0
    uint16_t wIndex = (XU_UNIT_ID << 8) | 0x00;

    int r = libusb_control_transfer(h, RT_GET, UVC_GET_CUR, wValue, wIndex,
                                    buf, len, 1000);
    if (r < 0) {
        printf("  sel %2u  %-12s  ERROR: %s\n",
               selector, label, libusb_error_name(r));
        return r;
    }
    hexdump(label, buf, r);
    return r;
}

int main() {
    libusb_context *ctx = nullptr;
    libusb_init(&ctx);

    libusb_device_handle *h = libusb_open_device_with_vid_pid(ctx, HIK_VID, HIK_PID);
    if (!h) {
        fprintf(stderr, "Камера не найдена (или нет прав)\n");
        return 1;
    }
    printf("Camera opened: %04x:%04x\n", HIK_VID, HIK_PID);
    printf("XU Unit %d, %d controls, control_size=%d\n\n",
           XU_UNIT_ID, XU_NUM_CONTROLS, CTRL_SIZE);

    // На Linux можно попробовать отвязать kernel driver
#ifdef __linux__
    if (libusb_kernel_driver_active(h, 0) == 1) {
        printf("Отвязываю kernel driver от interface 0\n");
        libusb_detach_kernel_driver(h, 0);
    }
#endif

    int r = libusb_claim_interface(h, 0);
    if (r < 0) {
        fprintf(stderr, "claim_interface(0) failed: %s\n", libusb_error_name(r));
        libusb_close(h);
        libusb_exit(ctx);
        return 1;
    }
    printf("interface 0 claimed\n\n");

    // Массив всех контролов
    for (int sel = 1; sel <= XU_NUM_CONTROLS; sel++) {
        uint8_t buf[64] = {0};

        printf("=== selector %d ===\n", sel);
        xu_get(h, sel, buf, CTRL_SIZE, "GET_CUR");
        xu_get(h, sel, buf, CTRL_SIZE, "GET_LEN");
        xu_get(h, sel, buf, CTRL_SIZE, "GET_INFO");
        xu_get(h, sel, buf, CTRL_SIZE, "GET_MIN");
        xu_get(h, sel, buf, CTRL_SIZE, "GET_MAX");
        xu_get(h, sel, buf, CTRL_SIZE, "GET_DEF");
        printf("\n");
    }

    libusb_release_interface(h, 0);
    libusb_close(h);
    libusb_exit(ctx);
    return 0;
}