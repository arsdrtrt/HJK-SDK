// 01_enum.cpp — перечислить USB-устройство HIK/HJK через libusb,
// показать дескрипторы без вендорского SDK.
//
// Сборка:
//   g++ -O2 -std=c++17 -o apps/01_enum apps/01_enum.cpp \
//       $(pkg-config --cflags --libs libusb-1.0)
//
// Запуск (нужен root или udev-правило):
//   sudo ./apps/01_enum

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <libusb-1.0/libusb.h>

#define HIK_VID 0x2bdf
#define HIK_PID 0x0102

static const char* speed_str(int s) {
    switch (s) {
        case LIBUSB_SPEED_LOW:   return "1.5 Mbps (low)";
        case LIBUSB_SPEED_FULL:  return "12 Mbps (full)";
        case LIBUSB_SPEED_HIGH:  return "480 Mbps (high)";
        case LIBUSB_SPEED_SUPER: return "5 Gbps (super)";
        default:                 return "unknown";
    }
}

static void dump_endpoint(const libusb_endpoint_descriptor *ep) {
    const char *dir = (ep->bEndpointAddress & 0x80) ? "IN " : "OUT";
    const char *type;
    switch (ep->bmAttributes & 0x03) {
        case LIBUSB_TRANSFER_TYPE_CONTROL:     type = "control"; break;
        case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS: type = "isoch"; break;
        case LIBUSB_TRANSFER_TYPE_BULK:        type = "bulk"; break;
        case LIBUSB_TRANSFER_TYPE_INTERRUPT:   type = "interrupt"; break;
        default:                               type = "?"; break;
    }
    printf("      EP 0x%02x  %s  %-9s  max_packet=%u",
           ep->bEndpointAddress, dir, type, ep->wMaxPacketSize);
    if ((ep->bmAttributes & 0x03) == LIBUSB_TRANSFER_TYPE_ISOCHRONOUS) {
        printf("  interval=%u", ep->bInterval);
    }
    if (ep->bInterval && (ep->bmAttributes & 0x03) == LIBUSB_TRANSFER_TYPE_INTERRUPT) {
        printf("  interval=%u", ep->bInterval);
    }
    printf("\n");
}

static void dump_config(const libusb_config_descriptor *cfg) {
    printf("  Configuration %u:\n", cfg->bConfigurationValue);
    printf("    interfaces: %u\n", cfg->bNumInterfaces);
    for (int i = 0; i < cfg->bNumInterfaces; i++) {
        const libusb_interface *iface = &cfg->interface[i];
        for (int j = 0; j < iface->num_altsetting; j++) {
            const libusb_interface_descriptor *id = &iface->altsetting[j];
            printf("    Interface %u alt %u:\n", id->bInterfaceNumber, id->bAlternateSetting);
            printf("      class=0x%02x subclass=0x%02x protocol=0x%02x\n",
                   id->bInterfaceClass, id->bInterfaceSubClass, id->bInterfaceProtocol);
            printf("      endpoints: %u\n", id->bNumEndpoints);
            for (int k = 0; k < id->bNumEndpoints; k++) {
                dump_endpoint(&id->endpoint[k]);
            }
        }
    }
}

int main() {
    libusb_context *ctx = nullptr;
    if (libusb_init(&ctx) < 0) {
        fprintf(stderr, "libusb_init failed\n");
        return 1;
    }

    libusb_device **list = nullptr;
    ssize_t n = libusb_get_device_list(ctx, &list);
    printf("libusb: %zd устройств найдено\n\n", n);

    int found = 0;
    for (ssize_t i = 0; i < n; i++) {
        libusb_device *dev = list[i];
        libusb_device_descriptor dd;
        if (libusb_get_device_descriptor(dev, &dd) < 0) continue;

        if (dd.idVendor != HIK_VID || dd.idProduct != HIK_PID) continue;

        found = 1;
        printf("=== HIK Camera %04x:%04x ===\n", dd.idVendor, dd.idProduct);
        printf("  bus=%u port=%u speed=%s\n",
               libusb_get_bus_number(dev),
               libusb_get_port_number(dev),
               speed_str(libusb_get_device_speed(dev)));
        printf("  bcdUSB=0x%04x bcdDevice=0x%04x\n", dd.bcdUSB, dd.bcdDevice);
        printf("  device class=0x%02x subclass=0x%02x protocol=0x%02x\n",
               dd.bDeviceClass, dd.bDeviceSubClass, dd.bDeviceProtocol);
        printf("  MaxPacketSize0=%u  configurations=%u\n",
               dd.bMaxPacketSize0, dd.bNumConfigurations);

        // Открыть и получить строки
        libusb_device_handle *h = nullptr;
        if (libusb_open(dev, &h) == 0) {
            unsigned char buf[256];
            auto print_str = [&](uint8_t idx, const char *label) {
                if (!idx) { printf("  %s: (none)\n", label); return; }
                int r = libusb_get_string_descriptor_ascii(h, idx, buf, sizeof(buf) - 1);
                if (r > 0) { buf[r] = 0; printf("  %s: %s\n", label, buf); }
                else       { printf("  %s: <error %d>\n", label, r); }
            };
            print_str(dd.iManufacturer, "Manufacturer");
            print_str(dd.iProduct,      "Product");
            print_str(dd.iSerialNumber, "Serial");
            libusb_close(h);
        }

        for (uint8_t c = 0; c < dd.bNumConfigurations; c++) {
            libusb_config_descriptor *cfg = nullptr;
            if (libusb_get_config_descriptor(dev, c, &cfg) == 0) {
                dump_config(cfg);
                libusb_free_config_descriptor(cfg);
            }
        }
        printf("\n");
    }

    if (!found) {
        printf("Камера %04x:%04x не найдена\n", HIK_VID, HIK_PID);
    }

    libusb_free_device_list(list, 1);
    libusb_exit(ctx);
    return found ? 0 : 1;
}
