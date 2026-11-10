#include <stdio.h>
#include <libusb.h>

#define REALTEK_VID 0x0BDA
#define CDROM_PID   0x1A2B

int main(void) {
    libusb_context *ctx = NULL;
    libusb_device_handle *handle = NULL;

    int result = libusb_init(&ctx);

    if (result != LIBUSB_SUCCESS) {
        fprintf(stderr, "Initialization failed: %s\n",
                libusb_error_name(result));
        return 1;
    }

    printf("=== RTL8821CU USB Access Test ===\n");

    handle = libusb_open_device_with_vid_pid(
        ctx, REALTEK_VID, CDROM_PID
    );

    if (handle == NULL) {
        printf("[FAIL] Could not open device 0bda:1a2b\n");
        printf("Device may be unavailable or access denied.\n");

        libusb_exit(ctx);
        return 1;
    }

    printf("[OK] Realtek USB device opened successfully.\n");
    
    int status = libusb_kernel_driver_active(handle, 0);

	if (status == 1) {
	    printf("[INFO] A kernel driver is active on interface 0.\n");
	} else if (status == 0) {
	    printf("[INFO] No active kernel driver reported.\n");
	} else {
	    printf("[INFO] Driver status unavailable: %s\n",
	           libusb_error_name(status));
	}
    
	printf("\n[TEST] Claiming USB interface 0...\n");

	int claim_result = libusb_claim_interface(handle, 0);

	if (claim_result == LIBUSB_SUCCESS) {
	    printf("[OK] Interface 0 claimed successfully.\n");

	    int release_result = libusb_release_interface(handle, 0);

	    if (release_result == LIBUSB_SUCCESS) {
	        printf("[OK] Interface 0 released successfully.\n");
	    } else {
	        printf("[WARN] Release failed: %s\n",
	               libusb_error_name(release_result));
	    }
	} else {
	    printf("[FAIL] Could not claim interface 0: %s\n",
	           libusb_error_name(claim_result));
	}

    libusb_close(handle);
    libusb_exit(ctx);

    printf("[OK] Device handle closed.\n");

    return 0;
}

