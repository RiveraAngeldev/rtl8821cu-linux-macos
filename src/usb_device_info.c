#include <stdio.h>
#include <libusb-1.0/libusb.h>

#define REALTEK_VID  0x0BDA
#define CDROM_PID    0x1A2B
#define WIFI_PID     0xC820

int main(void) {
    libusb_context *ctx = NULL;
    libusb_device **devices = NULL;

    int result = libusb_init(&ctx);

    if(result != LIBUSB_SUCCESS) {
        fprintf(stderr, "libusb initialization failed: %s\n",
                libusb_error_name(result));
        return 1;
    }

    ssize_t count = libusb_get_device_list(ctx, &devices);

    if(count < 0) {
        fprintf(stderr, "USB enumeration failed: %s\n",
                libusb_error_name((int)count));
        libusb_exit(ctx);
        return 1;
    }

    printf("====================================\n");
    printf("     RTL8821CU USB DEVICE INSPECTOR\n");
    printf("====================================\n\n");

    int found = 0;

    for(ssize_t i = 0; i < count; i++) {
        struct libusb_device_descriptor desc;

        if(libusb_get_device_descriptor(devices[i], &desc)
            != LIBUSB_SUCCESS) {
            continue;
        }

        if(desc.idVendor != REALTEK_VID) {
            continue;
        }

        if(desc.idProduct != CDROM_PID &&
            desc.idProduct != WIFI_PID) {
            continue;
        }

        found++;

        printf("Vendor ID:  0x%04x\n", desc.idVendor);
        printf("Product ID: 0x%04x\n", desc.idProduct);

        if(desc.idProduct == CDROM_PID) {
            printf("Mode:       CD-ROM / Installer\n");
        } else {
            printf("Mode:       Wi-Fi (802.11ac)\n");
        }

        printf("\n");
    }

    if(found == 0) {
        printf("Realtek adapter not detected.\n");
    }

    libusb_free_device_list(devices, 1);
    libusb_exit(ctx);

    return 0;
}
