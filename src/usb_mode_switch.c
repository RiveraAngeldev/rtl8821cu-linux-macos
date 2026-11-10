/*
 * RTL8821CU / Realtek 0bda:1a2b — USB BOT mode-switch experiment v6
 *
 * Reproduces the first two LUN-0 CBWs of usb_modeswitch StandardEject:
 *  1. ALLOW MEDIUM REMOVAL (0x1e)
 *  2. START STOP UNIT / EJECT (0x1b, LoEj=1, Start=0)
 * An SCSI Command Failed (CSW status 1) is not a BOT transport error,
 * and permits sending the next CBW after a validated CSW.
 * Stops on invalid CSW, phase errors, or any ambiguous transport state.
 * One attempt only; no USB reset, no driver detachment, no retry, no LUN 1.
 * --dry-run is the default. --execute requires explicit confirmation.
 * Strict device matching; never touches unrelated USB storage hardware.
 */
#include <libusb.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define TARGET_VID 0x0bda
#define TARGET_PID 0x1a2b
#define IFACE_NUM 0
#define EP_BULK_OUT 0x0b
#define EP_BULK_IN 0x8a
#define TIMEOUT_MS 2500

static void write_u32_le(unsigned char *p, uint32_t v) {
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

static uint32_t read_u32_le(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void dump_bytes(const char *label, const unsigned char *buf, int n) {
    printf("%s (%d bytes):\n", label, n);
    for (int i = 0; i < n; i++) {
        printf("%02x%s", (unsigned)buf[i], (i + 1 == n) ? "\n" : " ");
    }
    if (n == 0) putchar('\n');
}

static int check_layout(libusb_device *device) {
    struct libusb_device_descriptor dd;
    int rc = libusb_get_device_descriptor(device, &dd);
    if (rc != LIBUSB_SUCCESS) return rc;
    if (dd.idVendor != TARGET_VID || dd.idProduct != TARGET_PID)
        return LIBUSB_ERROR_NOT_FOUND;

    struct libusb_config_descriptor *cfg = NULL;
    rc = libusb_get_active_config_descriptor(device, &cfg);
    if (rc != LIBUSB_SUCCESS) return rc;

    bool ok = false;
    /* Require exactly one interface and the exact known endpoint pair. */
    if (cfg->bNumInterfaces == 1) {
        const struct libusb_interface *iface = &cfg->interface[0];
        for (int ai = 0; ai < iface->num_altsetting; ai++) {
            const struct libusb_interface_descriptor *a = &iface->altsetting[ai];
            if (a->bInterfaceNumber != IFACE_NUM || a->bAlternateSetting != 0 ||
                a->bInterfaceClass != LIBUSB_CLASS_MASS_STORAGE ||
                a->bInterfaceSubClass != 0x06 ||
                a->bInterfaceProtocol != 0x50 ||
                a->bNumEndpoints != 2)
                continue;
            bool seen_in = false, seen_out = false;
            for (int ep = 0; ep < a->bNumEndpoints; ep++) {
                const struct libusb_endpoint_descriptor *e = &a->endpoint[ep];
                if ((e->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) != LIBUSB_TRANSFER_TYPE_BULK)
                    continue;
                if (e->bEndpointAddress == EP_BULK_IN) seen_in = true;
                if (e->bEndpointAddress == EP_BULK_OUT) seen_out = true;
            }
            if (seen_in && seen_out) ok = true;
        }
    }
    libusb_free_config_descriptor(cfg);
    return ok ? LIBUSB_SUCCESS : LIBUSB_ERROR_OTHER;
}

/* The tag is an opaque correlation value. It need not be globally unique,
   but must match the returned CSW. A fresh timestamp-derived start tag helps
   distinguish captures from separate runs. */
static uint32_t fresh_tag(void) {
    uint32_t tag = (uint32_t)time(NULL) ^ UINT32_C(0x53c19a27);
    return tag ? tag : UINT32_C(0x11223344);
}

/* Return: 0 = valid CSW (status 0 or 1), 1 = transport uncertain/invalid,
   2 = target disappeared on the eject command; re-enumeration must be checked.
   status_out is set only for a valid CSW. */
static int scsi_zero_data(libusb_device_handle *handle, uint32_t tag,
                          unsigned char opcode, bool eject, unsigned *status_out) {
    unsigned char cbw[31] = {0};
    memcpy(cbw, "USBC", 4);
    write_u32_le(cbw + 4, tag);
    write_u32_le(cbw + 8, 0); /* No data stage */
    cbw[12] = 0;
    cbw[13] = 0; /* LUN 0 */
    cbw[14] = 6;
    cbw[15] = opcode;
    if (eject) cbw[19] = 0x02; /* START STOP UNIT: LoEj=1, Start=0 */

    printf("\n[COMMAND] %s (0x%02x), LUN 0\n",
           eject ? "START STOP UNIT / EJECT" : "ALLOW MEDIUM REMOVAL", opcode);
    printf("[DEBUG] Expected CSW tag: 0x%08x\n", (unsigned)tag);
    dump_bytes("[DEBUG] CBW OUT", cbw, 31);

    int transferred = 0;
    int rc = libusb_bulk_transfer(handle, EP_BULK_OUT, cbw, 31,
                                  &transferred, TIMEOUT_MS);
    printf("[USB] CBW OUT: %s, transferred=%d/31\n",
           libusb_error_name(rc), transferred);
    if (rc != LIBUSB_SUCCESS || transferred != 31) {
        puts("[STOP] CBW not fully confirmed; no further commands.");
        return (eject && rc == LIBUSB_ERROR_NO_DEVICE) ? 2 : 1;
    }

    unsigned char csw[13] = {0};
    transferred = 0;
    rc = libusb_bulk_transfer(handle, EP_BULK_IN, csw, 13,
                              &transferred, TIMEOUT_MS);
    printf("[USB] CSW IN: %s, transferred=%d/13\n",
           libusb_error_name(rc), transferred);
    if (transferred > 0 && transferred <= 13)
        dump_bytes("[DEBUG] CSW IN", csw, transferred);
    if (rc != LIBUSB_SUCCESS || transferred != 13) {
        puts("[STOP] CSW absent or incomplete; no more SCSI commands.");
        return (eject && rc == LIBUSB_ERROR_NO_DEVICE) ? 2 : 1;
    }

    uint32_t got_tag = read_u32_le(csw + 4);
    uint32_t residue = read_u32_le(csw + 8);
    unsigned status = csw[12];
    printf("[DEBUG] CSW tag: expected=0x%08x received=0x%08x\n",
           (unsigned)tag, (unsigned)got_tag);
    printf("[DEBUG] CSW residue=%u status=%u\n", (unsigned)residue, status);
    if (memcmp(csw, "USBS", 4) != 0 || got_tag != tag || residue != 0 || status > 2) {
        puts("[STOP] Invalid or inconsistent CSW. No further commands.");
        return 1;
    }
    if (status == 2) {
        puts("[STOP] BOT phase error; no reset/recovery attempted.");
        return 1;
    }
    *status_out = status;
    printf("[BOT] Valid CSW; SCSI command %s.\n",
           status == 0 ? "PASSED" : "FAILED (status 1)");
    return 0;
}

static void report_mode(libusb_context *ctx) {
    libusb_device **list = NULL;
    ssize_t n = libusb_get_device_list(ctx, &list);
    if (n < 0) {
        printf("[INFO] Post-test USB enumeration failed: %s\n", libusb_error_name((int)n));
        return;
    }
    int installer = 0, wifi = 0;
    for (ssize_t i = 0; i < n; ++i) {
        struct libusb_device_descriptor d;
        if (libusb_get_device_descriptor(list[i], &d) == LIBUSB_SUCCESS &&
            d.idVendor == TARGET_VID) {
            if (d.idProduct == TARGET_PID) installer++;
            if (d.idProduct == 0xc820) wifi++;
        }
    }
    libusb_free_device_list(list, 1);
    printf("[INFO] Enumeration: installer=%d, wifi-mode=%d\n", installer, wifi);
    if (wifi > 0) puts("[RESULT] Target Wi-Fi PID 0bda:c820 observed! Driver support on macOS is separate.");
    else puts("[INFO] Wi-Fi PID not observed. Check with usb_device_info after reconnection.");
}

int main(int argc, char **argv) {
    bool execute = false;
    if (argc == 2 && strcmp(argv[1], "--execute") == 0) execute = true;
    else if (argc == 1 || (argc == 2 && strcmp(argv[1], "--dry-run") == 0)) execute = false;
    else {
        fprintf(stderr, "Usage: %s [--dry-run | --execute]\n", argv[0]);
        return 2;
    }

    puts("=== RTL8821CU BOT Mode-Switch Experiment v6 ===");
    libusb_context *ctx = NULL;
    int rc = libusb_init(&ctx);
    if (rc != LIBUSB_SUCCESS) {
        printf("[FAIL] libusb_init: %s\n", libusb_error_name(rc));
        return 1;
    }
    int exit_code = 1;
    libusb_device **devices = NULL;
    ssize_t count = libusb_get_device_list(ctx, &devices);
    if (count < 0) {
        printf("[FAIL] USB enumeration: %s\n", libusb_error_name((int)count));
        goto cleanup;
    }
    libusb_device *target = NULL;
    int matches = 0;
    for (ssize_t i = 0; i < count; i++) {
        struct libusb_device_descriptor dd;
        if (libusb_get_device_descriptor(devices[i], &dd) == LIBUSB_SUCCESS &&
            dd.idVendor == TARGET_VID && dd.idProduct == TARGET_PID) {
            matches++;
            target = devices[i];
        }
    }
    if (matches != 1) {
        printf("[STOP] Expected exactly one 0bda:1a2b device; found %d.\n", matches);
        goto cleanup;
    }
    rc = check_layout(target);
    if (rc != LIBUSB_SUCCESS) {
        printf("[STOP] Interface/endpoint layout not approved: %s\n",
               libusb_error_name(rc));
        goto cleanup;
    }
    puts("[OK] Unique VID/PID; exact MSC BOT interface and endpoints verified.");
    puts("[PLAN] LUN 0: ALLOW MEDIUM REMOVAL (0x1e) -> EJECT (0x1b).");
    puts("[PLAN] Continue after a validated CSW with status 0 OR 1; stop on transport/phase errors.");
    puts("[SAFE] No LUN 1, no REQUEST SENSE, no USB reset, no driver detach, no retries.");
    if (!execute) {
        puts("[DRY-RUN] No interface claimed; no USB traffic sent.");
        exit_code = 0;
        goto cleanup;
    }

    puts("[WARNING] This sends REAL SCSI commands and may disconnect/re-enumerate the adapter.");
    puts("[WARNING] Do not run if the device is mounted or being used.");
    printf("Type EJECT 0bda:1a2b to proceed: ");
    fflush(stdout);
    char reply[64];
    if (!fgets(reply, sizeof(reply), stdin) || strcmp(reply, "EJECT 0bda:1a2b\n") != 0) {
        puts("[CANCELLED] Confirmation did not match.");
        goto cleanup;
    }

    libusb_device_handle *handle = NULL;
    rc = libusb_open(target, &handle);
    if (rc != LIBUSB_SUCCESS) {
        printf("[STOP] Could not open target: %s\n", libusb_error_name(rc));
        goto cleanup;
    }
    int driver = libusb_kernel_driver_active(handle, IFACE_NUM);
    if (driver == 1) {
        puts("[STOP] Kernel driver active on interface 0; no detach attempted.");
        libusb_close(handle);
        goto cleanup;
    }
    if (driver < 0 && driver != LIBUSB_ERROR_NOT_SUPPORTED) {
        printf("[STOP] Driver state unknown: %s\n", libusb_error_name(driver));
        libusb_close(handle);
        goto cleanup;
    }
    rc = libusb_claim_interface(handle, IFACE_NUM);
    if (rc != LIBUSB_SUCCESS) {
        printf("[STOP] Claim failed: %s\n", libusb_error_name(rc));
        libusb_close(handle);
        goto cleanup;
    }
    puts("[OK] Interface 0 claimed.");
    uint32_t tag = fresh_tag();
    unsigned status = 0;
    int first = scsi_zero_data(handle, tag, 0x1e, false, &status);
    if (first == 0) {
        if (status == 1)
            puts("[INFO] 0x1e was rejected at SCSI level, but BOT is synchronized.");
        puts("[INFO] Following standard eject sequence with LUN 0 EJECT.");
        unsigned eject_status = 0;
        int second = scsi_zero_data(handle, tag + 1, 0x1b, true, &eject_status);
        if (second == 0) {
            puts(eject_status == 0 ?
                 "[RESULT] EJECT command accepted; checking enumeration." :
                 "[RESULT] EJECT command rejected by SCSI; checking enumeration.");
            exit_code = eject_status == 0 ? 0 : 1;
        } else {
            puts("[INFO] EJECT transport outcome uncertain; checking actual USB ID.");
            exit_code = 1;
        }
    } else {
        puts("[STOP] First BOT transaction invalid or incomplete; EJECT NOT sent.");
        exit_code = 1;
    }
    int release_rc = libusb_release_interface(handle, IFACE_NUM);
    if (release_rc != LIBUSB_SUCCESS)
        printf("[WARN] Release: %s\n", libusb_error_name(release_rc));
    else
        puts("[OK] Interface 0 released.");
    libusb_close(handle);
    puts("[INFO] Device handle closed.");
    /* USB re-enumeration can lag behind the command; observation only. */
    for (int check = 0; check < 3; ++check) {
        if (check > 0) sleep(2);
        report_mode(ctx);
    }
cleanup:
    if (devices) libusb_free_device_list(devices, 1);
    libusb_exit(ctx);
    return exit_code;
}
