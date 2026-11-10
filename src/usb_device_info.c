#include <stdio.h>
#include <sys/types.h>
#include <libusb.h>

#define REALTEK_VID 0x0BDA
#define CDROM_PID   0x1A2B
#define WIFI_PID    0xC820

const char *transfer_type_name(unsigned char attributes) {
    switch (attributes & LIBUSB_TRANSFER_TYPE_MASK) {
        case LIBUSB_TRANSFER_TYPE_CONTROL:
            return "Control";
        case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS:
            return "Isochronous";
        case LIBUSB_TRANSFER_TYPE_BULK:
            return "Bulk";
        case LIBUSB_TRANSFER_TYPE_INTERRUPT:
            return "Interrupt";
        default:
            return "Unknown";
    }
}

void inspect_usb_interfaces(libusb_device *device) {
    struct libusb_config_descriptor *config = NULL;

    int result = libusb_get_active_config_descriptor(
        device, &config
    );

    if (result != LIBUSB_SUCCESS) {
        fprintf(stderr, "Configuration error: %s\n",
                libusb_error_name(result));
        return;
    }

    printf("\n--- USB Configuration ---\n");
    printf("Interfaces: %u\n", config->bNumInterfaces);

    for (int i = 0; i < config->bNumInterfaces; i++) {

        const struct libusb_interface *iface =
            &config->interface[i];

        for (int j = 0; j < iface->num_altsetting; j++) {

            const struct libusb_interface_descriptor *alt =
                &iface->altsetting[j];

            printf("\nInterface %u (Alt %u)\n",
                   alt->bInterfaceNumber,
                   alt->bAlternateSetting);

            printf("  Class: 0x%02x\n",
                   alt->bInterfaceClass);

            printf("  Endpoints: %u\n",
                   alt->bNumEndpoints);

            for (int k = 0; k < alt->bNumEndpoints; k++) {

                const struct libusb_endpoint_descriptor *ep =
                    &alt->endpoint[k];

                printf("    Endpoint: 0x%02x\n",
                       ep->bEndpointAddress);

                printf("      Direction: %s\n",
                       (ep->bEndpointAddress & LIBUSB_ENDPOINT_IN)
                       ? "IN" : "OUT");

                printf("      Type: %s\n",
                       transfer_type_name(ep->bmAttributes));

                printf("      Max Packet Size: %u bytes\n",
                       ep->wMaxPacketSize);
            }
        }
    }

    libusb_free_config_descriptor(config);
}

int main(void) {
    libusb_context *ctx = NULL;
    libusb_device **devices = NULL;

    int result = libusb_init(&ctx);

    if (result != LIBUSB_SUCCESS) {
        fprintf(stderr, "libusb initialization failed: %s\n",
                libusb_error_name(result));
        return 1;
    }

    ssize_t count = libusb_get_device_list(ctx, &devices);

    if (count < 0) {
        fprintf(stderr, "USB enumeration failed: %s\n",
                libusb_error_name((int)count));
        libusb_exit(ctx);
        return 1;
    }

    printf("====================================\n");
    printf("     RTL8821CU USB DEVICE INSPECTOR\n");
    printf("====================================\n\n");

    int found = 0;

    for (ssize_t i = 0; i < count; i++) {

        struct libusb_device_descriptor desc;

        if (libusb_get_device_descriptor(devices[i], &desc)
            != LIBUSB_SUCCESS) {
            continue;
        }

        if (desc.idVendor != REALTEK_VID ||
            (desc.idProduct != CDROM_PID &&
             desc.idProduct != WIFI_PID)) {
            continue;
        }

        found++;

        printf("Vendor ID:  0x%04x\n", desc.idVendor);
        printf("Product ID: 0x%04x\n", desc.idProduct);

        printf("USB Bus:    %u\n",
               libusb_get_bus_number(devices[i]));

        printf("USB Device: %u\n",
               libusb_get_device_address(devices[i]));

        if (desc.idProduct == CDROM_PID) {
            printf("Mode:       CD-ROM / Installer\n");
        } else {
            printf("Mode:       Wi-Fi (802.11ac)\n");
        }

        inspect_usb_interfaces(devices[i]);
        printf("\n");
    }

    if (found == 0) {
        printf("Realtek adapter not detected.\n");
    }

    libusb_free_device_list(devices, 1);
    libusb_exit(ctx);

    return 0;
}
