# RTL8821CU — Linux & macOS USB Wi-Fi Research

> A systems programming research project documenting Realtek USB device identification, USB mode switching, Linux kernel driver verification, and future cross-platform C/`libusb` development.

**Current stage:** Cross-platform USB Inspector v0.2 verified; macOS USB access and BOT diagnostics implemented in C. Diagnostic v5 is prepared but its execution has not been verified.  
**Observed transition:** `0bda:1a2b` (virtual CD-ROM mode) → `0bda:c820` (802.11ac mode).  
**Verified:** Linux driver binding and 2.4/5 GHz wireless scanning; cross-platform USB descriptor enumeration; on macOS, device open, interface claim/release, a BOT CBW send and CSW reception.  
**Not yet verified:** Wi-Fi association, packet transfer, Bluetooth pairing/audio, successful macOS mode switching, or native macOS Wi-Fi networking. The adapter remains in installation mode on macOS.

> **Privacy notice:** This public version is intentionally sanitized. Hostnames, usernames, network names (SSIDs), network interface identifiers derived from MAC addresses, IP addresses, and raw scan data are excluded. Terminal excerpts are either limited to relevant lines or labeled *sanitized*. The adapter's USB VID:PID values and driver names are retained because they are needed to reproduce the hardware investigation.

## Contents

- [Project scope](#project-scope)
- [Tested hardware and environment](#tested-hardware-and-environment)
- [Development environment setup](#development-environment-setup)
- [Phase 1 — Device identification](#phase-1--device-identification)
- [Phase 2 — USB mode switching](#phase-2--usb-mode-switching)
- [Phase 3 — Driver verification](#phase-3--driver-verification)
- [Phase 4 — Wireless scanning](#phase-4--wireless-scanning)
- [Phase 5 — USB Device Inspector v0.1 (C / libusb)](#phase-5--usb-device-inspector-v01-c--libusb)
- [Phase 6 — USB Device Inspector v0.2 (interfaces and endpoints)](#phase-6--usb-device-inspector-v02-interfaces-and-endpoints)
- [Phase 7 — macOS cross-platform validation](#phase-7--macos-cross-platform-validation)
- [Phase 8 — macOS USB access and BOT diagnostics](#phase-8--macos-usb-access-and-bot-diagnostics)
- [Reproducing the checks safely](#reproducing-the-checks-safely)
- [Development roadmap](#development-roadmap)
- [Privacy and evidence policy](#privacy-and-evidence-policy)
- [References](#references)

## Project scope

Some USB Wi-Fi adapters initially present themselves as a **virtual CD-ROM / mass-storage device** containing installation software. They must then re-enumerate under another USB product identifier before an operating system can use the wireless interface.

This repository tracks the investigation of a Realtek USB Wi-Fi adapter on Debian Linux and macOS. The first milestone is a portable, read-only USB inspector in **C / `libusb`**. The separate `usb_mode_switch.c` experiment can transmit USB Mass Storage commands only with an explicit `--execute` option. Further work includes analysis of the **Linux `rtw88` driver** and research into constraints affecting **macOS** support.

This project **does not contain a custom Wi-Fi driver** and does not establish that native macOS Wi-Fi support is possible for this adapter. The inspector and BOT diagnostic are separate userspace programs; this repository does not implement a network driver.

## Tested hardware and environment

The same physical Realtek adapter was examined on **Debian Linux** and **macOS Monterey**. The operating systems observed different USB modes; the tables distinguish actual test results from goals that have not yet been demonstrated.

### Debian Linux

| Property | Observed result |
| --- | --- |
| Operating system | Debian Linux 13 (Trixie) |
| Kernel family | Linux 6.12 series |
| USB manufacturer | Realtek (`0bda`) |
| Initial USB VID:PID | `0bda:1a2b` — virtual CD-ROM / installer mode |
| Initial USB description | `RTL8188GU 802.11n WLAN Adapter (Driver CDROM Mode)` |
| Later observed VID:PID | `0bda:c820` — `802.11ac NIC` |
| USB mode-switching tool | `usb_modeswitch` 2.6.1 |
| Kernel module | `rtw88_8821cu` |
| Bound USB driver | `rtw_8821cu` |
| Operational-mode interfaces | 3: Bluetooth (0, 1) and Wi-Fi (2) |
| Wi-Fi scanning | Confirmed on 2.4 GHz and 5 GHz |
| C / `libusb` inspector | v0.2 compiled and executed successfully |
| Wi-Fi association / traffic | Not tested |
| Mode-switch trigger | Re-enumeration observed, but the exact triggering event was not isolated |

### macOS Monterey

| Property | Observed result |
| --- | --- |
| Operating system | macOS Monterey 12.7.6 |
| Computer architecture | Intel (`x86_64`) |
| C compiler | Apple Clang (`/usr/bin/clang`) |
| Package manager for `libusb` | Homebrew (`/usr/local/bin/brew`) |
| `libusb` version | 1.0.30 |
| `pkg-config` observed | MacPorts executable (`/opt/local/bin/pkg-config`) |
| Build environment issue | `pkg-config` initially could not locate Homebrew's `libusb-1.0.pc` |
| Verified build workaround | Set `PKG_CONFIG_PATH` to Homebrew `libusb`'s `lib/pkgconfig` directory in the active terminal |
| Detected USB VID:PID | `0bda:1a2b` — virtual CD-ROM / installer mode |
| macOS USB device label | `DISK` (Realtek) |
| Installer USB configuration | 1 interface: 0, Mass Storage `0x08`, SCSI subclass `0x06`, BOT protocol `0x50` |
| Installer endpoints | Bulk IN `0x8a`; Bulk OUT `0x0b`; 512-byte maximum USB packets |
| C / `libusb` inspector | v0.2 compiled and executed successfully |
| `libusb` device access | Open and close verified |
| Interface ownership | Driver initially reported active; subsequently no active driver reported after USB client applications were closed |
| Interface access | Interface 0 successfully claimed and released |
| BOT command tests | CBW sent; CSW received with `status=1` for SCSI `0x1e` (earlier versions) |
| `REQUEST SENSE` experiment | A 13-byte CSW, rather than expected sense data, was observed (v4) |
| Latest diagnostic source | `src/usb_mode_switch.c` v5; execution result not yet supplied |
| USB mode switch to `0bda:c820` | Not verified; adapter remained in installation mode |
| Native macOS Wi-Fi networking | Not implemented or verified |

**Identification caveat:** The `RTL8188GU` label is associated with the installation-mode ID and is not sufficient to identify the operational chipset. The later `0bda:c820` device matched the Linux `rtw88_8821cu` driver and its running USB binding.

**Privacy:** Temporary USB bus/device numbers, registry session identifiers, device serials, private network information, hostnames, and local user account names are intentionally omitted from these environment tables.

## Development environment setup

The repository includes an OS-aware bootstrap script that installs **userspace development and diagnostic tools**, not Wi-Fi kernel drivers.

```bash
# Inspect the commands before making changes
bash scripts/setup.sh --dry-run

# Install missing dependencies (prompts for normal package-manager permissions)
bash scripts/setup.sh

# Check already installed packages, without changing the system
bash scripts/setup.sh --check
```

| Platform | Package manager | Dependencies |
| --- | --- | --- |
| Debian/Ubuntu Linux | APT | `usb-modeswitch`, `usb-modeswitch-data`, `usbutils`, `iw`, `kmod`, `build-essential`, `pkg-config`, `libusb-1.0-0-dev`, `ripgrep` |
| macOS | Homebrew | `libusb`, `pkgconf` (with Apple Xcode Command Line Tools providing the C compiler) |

**Notes:** Homebrew is a prerequisite on macOS and is never downloaded or installed automatically by this script. If Apple Command Line Tools are absent, follow `xcode-select --install`. The Linux-only utilities (`iw`, `kmod`, and `usb-modeswitch`) do **not** have direct equivalent roles on macOS and are not installed there. On macOS, `system_profiler SPUSBDataType` can be used for native USB inspection. This script does not enable a macOS Wi-Fi driver or guarantee support on older macOS releases.

The script is repeatable: it checks which packages are installed before attempting installations, and the `--dry-run` option makes no changes. It does **not** compile programs or transmit USB commands. Compile each userspace utility with the commands documented in its phase.

### macOS: fix missing `libusb-1.0.pc` and `libusb.h` during compilation

When building `src/usb_mode_switch.c` (or `src/usb_device_info.c`), you may see:

```text
Package libusb-1.0 was not found in the pkg-config search path.
No package 'libusb-1.0' found
src/usb_mode_switch.c:13:10: fatal error: 'libusb.h' file not found
```

On the tested Intel Mac, **Homebrew installed `libusb`, but `pkg-config` came from MacPorts** and did not search Homebrew's metadata directory. Verify installation and set the search path in **the same terminal session used for compiling**:

```bash
brew list libusb || brew install libusb
export PKG_CONFIG_PATH="$(brew --prefix libusb)/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
pkg-config --modversion libusb-1.0
pkg-config --cflags --libs libusb-1.0
```

Then compile:

```bash
mkdir -p build
clang -std=c11 -Wall -Wextra -Wpedantic -O2 \
  src/usb_mode_switch.c \
  -o build/usb_mode_switch \
  $(pkg-config --cflags --libs libusb-1.0)
```

The missing `libusb.h` error can result from omitted compiler include flags when `pkg-config` fails, rather than from a missing header file. If `brew --prefix libusb` fails, install Homebrew `libusb` first. `scripts/setup.sh` configures this environment **inside the script** for its checks, but a script cannot permanently change the parent shell's `PKG_CONFIG_PATH`; interactive compilation still needs the `export` above.

## Phase 1 — Device identification

### Why use `lsusb`?

```bash
lsusb
```

`lsusb` lists the USB devices recognized by Linux. Its **Vendor ID (VID)** and **Product ID (PID)** fields let us distinguish the adapter's installation mode from its operational mode.

**Initial relevant output:**

```text
Bus 001 Device 004: ID 0bda:1a2b Realtek Semiconductor Corp. RTL8188GU 802.11n WLAN Adapter (Driver CDROM Mode)
```

We also inspected the USB topology:

```bash
lsusb -t
```

**Relevant interface entry (excerpt):**

```text
If 0, Class=Mass Storage, Driver=[none], 480M
```

The kernel logs confirmed that the device presented itself as a storage unit:

```bash
sudo journalctl -k -b --no-pager
```

**Relevant excerpt:**

```text
New USB device found, idVendor=0bda, idProduct=1a2b
Product: DISK
Manufacturer: Realtek
USB Mass Storage device detected
```

### Figure 1 — Initial CD-ROM mode

![Figure 1 — Realtek USB adapter initially recognized in CD-ROM mode](docs/images/01-initial-cdrom-mode.png)

*Figure 1. The first `usb_modeswitch` attempt and subsequent USB inspection. The adapter still appeared as `0bda:1a2b` / Mass Storage at that point.*

## Phase 2 — USB mode switching

The `usb-modeswitch` utility can trigger a device to stop presenting its installation interface and re-enumerate in its operational mode.

### Tools used

```bash
sudo apt update
sudo apt install usb-modeswitch usb-modeswitch-data iw kmod
```

| Package | Purpose |
| --- | --- |
| `usb-modeswitch` | Send supported mode-switching commands to multi-mode USB devices |
| `usb-modeswitch-data` | Provide known device rules and configurations |
| `iw` | Inspect and scan Linux wireless interfaces |
| `kmod` | Supply kernel module tools including `modinfo` and `lsmod` |

The initial attempt was:

```bash
sudo usb_modeswitch -K -v 0bda -p 1a2b
```

| Flag | Purpose |
| --- | --- |
| `-K` | Send the standard mass-storage eject sequence |
| `-v 0bda` | Match the USB vendor ID |
| `-p 1a2b` | Match the USB product ID in installation mode |

**Observed output:**

```text
Sending standard EJECT sequence
Sending the message returned error -7. Try to continue
Response reading failed (error -7)
Device is gone, skip any further commands
```

In `libusb`, error `-7` indicates a **timeout**. A follow-up `lsusb` still showed `0bda:1a2b`, so this attempt alone **did not establish a successful mode switch**.

At a later observation, `lsusb` reported:

```text
Bus 001 Device 006: ID 0bda:c820 Realtek Semiconductor Corp. 802.11ac NIC
```

The exact event that caused this successful re-enumeration **was not isolated**, so the project does not attribute it conclusively to any particular command or reconnection.

A later diagnostic command:

```bash
sudo usb_modeswitch -v 0bda -p 1a2b -KW
```

returned:

```text
No devices in default mode found. Nothing to do. Bye!
```

This was expected because the adapter was **already** enumerated under `0bda:c820`. The `-W` flag requests verbose output; that command did not itself perform the observed successful transition.

### Figure 2 — Operational Wi-Fi mode

![Figure 2 — Realtek USB adapter identified as an 802.11ac network device](docs/images/02-wifi-mode-detected.png)

*Figure 2. `lsusb` shows the operational USB ID `0bda:c820`. The subsequent verbose mode-switch check finds no installation-mode device.*

## Phase 3 — Driver verification

### Confirm the module installed for this adapter

```bash
modinfo rtw88_8821cu
```

**Selected, sanitized module metadata:**

```text
license:        Dual BSD/GPL
description:    Realtek 802.11ac wireless 8821cu driver
alias:          usb:v0BDApC820d*dc*dsc*dp*icFFiscFFipFFin*
depends:        usbcore,rtw88_8821c,rtw88_usb
intree:         Y
name:           rtw88_8821cu
```

The module advertises compatibility with `0bda:c820`. `intree: Y` means it was built from the kernel tree.

### Verify the USB driver binding

```bash
lsusb -t
```

**Relevant lines only (other host devices omitted):**

```text
If 0, Class=Wireless, Driver=btusb, 480M
If 1, Class=Wireless, Driver=btusb, 480M
If 2, Class=Vendor Specific Class, Driver=rtw_8821cu, 480M
```

The adapter exposes Bluetooth USB interfaces (0 and 1) alongside its Wi-Fi interface (2). This confirms driver binding, **not** that Bluetooth pairing has been tested.

The associated network-device driver was also verified through Linux sysfs. The command used a local interface name that is intentionally omitted from the public record:

```bash
# Substitute the interface name found locally using: iw dev
readlink -f "/sys/class/net/${USB_WIFI_IFACE}/device/driver"
```

**Observed output:**

```text
/sys/bus/usb/drivers/rtw_8821cu
```

### Verify loaded kernel modules

```bash
lsmod | rg 'rtw88|rtw_'
```

**Relevant module names from the recorded output (host-specific counts omitted):**

```text
rtw88_8821cu
rtw88_8821c
rtw88_usb
rtw88_core
mac80211
cfg80211
usbcore
```

`rtw88_8821cu` identifies the USB adapter, `rtw88_usb` handles USB transport, and `rtw88_8821c` supplies chipset-specific support. The Linux wireless stack uses `mac80211` and `cfg80211`.

> `lsmod` reference counts are not an activity indicator. The binding was confirmed independently through `lsusb -t` and sysfs.

### Optional Figure 3 — Driver binding

This result is already documented above. If a screenshot is added, **capture only the adapter's interface lines and the final sysfs driver path**; crop out the rest of the USB hardware inventory, device name, prompts, and paths containing a MAC-derived interface name.

Suggested public filename: `docs/images/03-kernel-driver-binding.png`.

<!-- Add a Markdown image link here ONLY after creating and reviewing a sanitized screenshot. -->

## Phase 4 — Wireless scanning

Running `iw dev` confirmed the presence of both an internal Wi-Fi interface and a separate USB wireless interface in **managed** mode. Their actual names, MAC addresses, and associated SSID have been omitted intentionally.

We then used the **USB adapter only** to scan for nearby wireless networks:

```bash
# USB_WIFI_IFACE must be assigned locally, not committed to Git.
sudo iw dev "$USB_WIFI_IFACE" scan
```

**Selected terminal observations — sanitized (SSID and BSSID omitted):**

```text
freq: 2452.0
signal: -58.00 dBm
freq: 5805.0
signal: -60.00 dBm
freq: 2432.0
signal: -62.00 dBm
freq: 5200.0
signal: -55.00 dBm
```

The real scan returned results on **2.4 GHz and 5 GHz**, confirming the adapter's scanning / reception path. Frequencies and signal strengths are retained as technical measurements; network identifiers are not published.

**Verified:** receiving scan results. **Not verified:** association, authentication, DHCP, packet transfer, throughput, or Bluetooth operation.

### Optional Figure 4 — Wireless scan

If a screenshot is needed, use a command that excludes network names and addresses from the outset:

```bash
sudo iw dev "$USB_WIFI_IFACE" scan | rg 'freq:|signal:'
```

Suggested public filename: `docs/images/04-wireless-scan.png`.

<!-- Add a Markdown image link here ONLY after creating and reviewing a sanitized screenshot. -->

## Phase 5 — USB Device Inspector v0.1 (C / libusb)

This phase marks the **first code written for the repository**. The goal is to identify the adapter directly through the `libusb` API, without invoking `lsusb` or replacing the existing Linux Wi-Fi driver.

### Source code

**File:** [`src/usb_device_info.c`](src/usb_device_info.c)

The initial implementation:

1. Initializes a `libusb` context with `libusb_init()`.
2. Enumerates attached USB devices using `libusb_get_device_list()`.
3. Reads each device descriptor via `libusb_get_device_descriptor()`.
4. Filters for vendor `0x0bda` and product `0x1a2b` (installer) or `0xc820` (Wi-Fi).
5. Prints the observed VID, PID, and matching device mode.
6. Releases the device list and `libusb` context before exiting.

**Limitations:** v0.1 identifies the adapter from known USB IDs only. It does not claim USB interfaces, transmit commands, scan wireless networks, modify firmware, or implement a Wi-Fi driver.

### Build requirements (Debian Linux)

```bash
sudo apt update
sudo apt install build-essential pkg-config libusb-1.0-0-dev
```

### Build

Run these commands from the repository root:

```bash
mkdir -p build
gcc -std=c11 -Wall -Wextra -O2 \
    src/usb_device_info.c \
    -o build/usb_device_info \
    $(pkg-config --cflags --libs libusb-1.0)
```

### Run

```bash
./build/usb_device_info
```

**Observed terminal result (Debian Linux, v0.1):**

```text
====================================
     RTL8821CU USB DEVICE INSPECTOR
====================================

Vendor ID:  0x0bda
Product ID: 0xc820
Mode:       Wi-Fi (802.11ac)
```

**Result:** The program compiled and successfully identified the USB Wi-Fi adapter. This verifies basic device enumeration and descriptor-based identification using `libusb`. It does **not** verify network connectivity.

No screenshot is necessary here: the sanitized terminal output above is sufficient evidence and does not reveal hostnames, usernames, MAC addresses, or nearby networks.

### Cross-platform status

`libusb` supports several operating systems, including macOS. **The same inspector source was subsequently compiled and executed on macOS Monterey** (Phase 7). USB descriptor access does not imply a functional macOS Wi-Fi driver.

### Next development milestone (as planned after v0.1)

- Inspect USB configurations, interfaces, and endpoints (Inspector v0.2) — **completed in Phase 6**.
- Add structured error handling and tests.
- Keep development tools separate from the kernel driver.

## Phase 6 — USB Device Inspector v0.2 (interfaces and endpoints)

Version **v0.2** extends [`src/usb_device_info.c`](src/usb_device_info.c) beyond VID/PID detection to inspect the adapter's **active USB configuration**, interface descriptors, alternate settings, and endpoint descriptors. The program was first compiled and executed successfully on Debian Linux; it was subsequently compiled and executed on macOS Monterey (Phase 7).

### Implementation

- `libusb_get_active_config_descriptor()` obtains the descriptor of the active configuration without claiming an interface.
- `config->bNumInterfaces` gives the number of USB interfaces.
- `config->interface[i].altsetting[j]` describes each interface's alternate settings.
- `bEndpointAddress` indicates endpoint number and direction (IN/OUT).
- `bmAttributes & LIBUSB_TRANSFER_TYPE_MASK` identifies the transfer type (Control, Isochronous, Bulk, or Interrupt).
- `wMaxPacketSize` reports the endpoint's maximum USB packet size (not Wi-Fi throughput).
- `libusb_free_config_descriptor()` releases the allocated configuration descriptor.

This is still a **read-only descriptor inspector**, not a custom Wi-Fi driver. No kernel interfaces are detached, no firmware is modified, and no USB data transfers are initiated by this tool.

### Build and run

From the repository root:

```bash
mkdir -p build
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 \
    src/usb_device_info.c \
    -o build/usb_device_info \
    $(pkg-config --cflags --libs libusb-1.0)
./build/usb_device_info
```

### Verified device summary

**Selected output from the real run (no private network identifiers):**

```text
RTL8821CU USB DEVICE INSPECTOR
Vendor ID:  0x0bda
Product ID: 0xc820
USB Bus:    1
USB Device: 6
Mode:       Wi-Fi (802.11ac)

--- USB Configuration ---
Interfaces: 3
```

Bus and device numbers are temporary enumeration values and may change on reconnect.

| USB interface | Alternate settings observed | Class | Driver observed earlier with `lsusb -t` | Purpose |
| --- | --- | --- | --- | --- |
| 0 | 0 | `0xe0` | `btusb` | Bluetooth transport |
| 1 | 0–5 | `0xe0` | `btusb` | Bluetooth isochronous transport |
| 2 | 0 | `0xff` (vendor-specific) | `rtw_8821cu` | Wi-Fi transport |

### Wi-Fi USB endpoints — interface 2, alternate setting 0

**The following values were read from the device, not inferred or invented:**

```text
Interface 2 (Alt 0)
  Class: 0xff
  Endpoints: 5
    Endpoint: 0x84
      Direction: IN
      Type: Bulk
      Max Packet Size: 512 bytes
    Endpoint: 0x05
      Direction: OUT
      Type: Bulk
      Max Packet Size: 512 bytes
    Endpoint: 0x06
      Direction: OUT
      Type: Bulk
      Max Packet Size: 512 bytes
    Endpoint: 0x87
      Direction: IN
      Type: Interrupt
      Max Packet Size: 64 bytes
    Endpoint: 0x08
      Direction: OUT
      Type: Bulk
      Max Packet Size: 512 bytes
```

| Address | Direction | Transfer type | Maximum USB packet size |
| --- | --- | --- | --- |
| `0x84` | Device → host (IN) | Bulk | 512 bytes |
| `0x05` | Host → device (OUT) | Bulk | 512 bytes |
| `0x06` | Host → device (OUT) | Bulk | 512 bytes |
| `0x87` | Device → host (IN) | Interrupt | 64 bytes |
| `0x08` | Host → device (OUT) | Bulk | 512 bytes |

Bulk endpoints provide reliable transfers, while interrupt endpoints support scheduled polling for notifications/events. The endpoint descriptors alone do **not** establish which internal Wi-Fi queue or firmware message each endpoint handles; that requires studying the `rtw88` driver source.

### Bluetooth interface observations

**Interface 0 (Alt 0)** exposed three endpoints:

```text
0x81  IN   Interrupt   16 bytes
0x02  OUT  Bulk       512 bytes
0x82  IN   Bulk       512 bytes
```

**Interface 1** exposed two isochronous endpoints, `0x03` (OUT) and `0x83` (IN), under each of six alternate settings:

| Alternate setting | `0x03` OUT max packet | `0x83` IN max packet |
| --- | --- | --- |
| 0 | 0 bytes | 0 bytes |
| 1 | 9 bytes | 9 bytes |
| 2 | 17 bytes | 17 bytes |
| 3 | 25 bytes | 25 bytes |
| 4 | 33 bytes | 33 bytes |
| 5 | 49 bytes | 49 bytes |

Alternate settings are **mutually exclusive configurations of one interface**; these values do not mean that all twelve listed endpoint descriptors are active at once. The results confirm descriptor visibility only, **not functional Bluetooth audio or pairing**.

### Evidence and privacy

The tables and terminal excerpts above are sufficient documentation for v0.2; **no new screenshot is required**. They contain USB IDs, endpoint addresses, classes, and packet sizes, but no SSIDs, BSSIDs, MAC addresses, IP addresses, hostnames, or user accounts. Avoid uploading a raw terminal capture if it includes additional system information.

### Status and next steps

- **Verified:** v0.2 compiles and enumerates three interfaces, including five Wi-Fi endpoints on interface 2.
- **Not verified:** Wi-Fi association, packet transfer, Bluetooth pairing/audio, or macOS Wi-Fi networking. Descriptor inspection on macOS is verified in Phase 7.
- **Next:** refactor the inspector into smaller functions, add CLI options (`--help`, `--verbose`), and continue cross-platform tests before attempting any kernel-driver modifications.

## Phase 7 — macOS cross-platform validation

This phase tests **the existing USB Device Inspector v0.2 on macOS** without modifying the laptop's Wi-Fi drivers, kernel security settings, or unrelated USB storage devices. It validates portable USB **enumeration and descriptor inspection**, not Wi-Fi connectivity.

### Test environment

| Property | Observed value |
| --- | --- |
| Operating system | macOS Monterey 12.7.6 |
| Architecture | Intel (`x86_64`) |
| C compiler | Apple Clang (`/usr/bin/clang`) |
| Homebrew executable | `/usr/local/bin/brew` |
| `pkg-config` executable | `/opt/local/bin/pkg-config` |
| Homebrew `libusb` | 1.0.30 |
| Realtek device ID on macOS | `0bda:1a2b` (CD-ROM / installer mode) |

These details identify the development environment, without exposing any hostname, account name, device serial number, or private network information.

### Initial USB detection

The command below was used to inspect macOS USB enumeration:

```bash
system_profiler SPUSBDataType
```

**Selected, sanitized output:**

```text
DISK:
    Product ID: 0x1a2b
    Vendor ID: 0x0bda (Realtek Semiconductor Corp.)
    Manufacturer: Realtek
    Speed: Up to 480 Mb/s
```

macOS detected the Realtek device in its installation mode. A separate `diskutil list external` check listed an unrelated external storage device, **not an identified Realtek virtual disk**. No disk-eject operation was performed.

### Dependency troubleshooting: Homebrew and MacPorts

The repository's `scripts/setup.sh --dry-run` correctly detected Darwin and proposed Homebrew packages:

```text
[INFO] Detected operating system: Darwin
Would run: brew install libusb pkgconf
```

The initial installer run installed `libusb` through Homebrew, but its build-environment verification failed because `pkg-config` did not find `libusb-1.0.pc`. The observed tools were in **different package-manager prefixes**: `pkg-config` at `/opt/local/bin` and Homebrew `libusb` under `/usr/local`.

The following **session-local workaround was successfully tested**:

```bash
export PKG_CONFIG_PATH="$(brew --prefix libusb)/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
pkg-config --modversion libusb-1.0
pkg-config --cflags --libs libusb-1.0
```

**Observed version:** `1.0.30`. The flags referenced the Homebrew `libusb` include and library directories. This environment variable is a shell-session configuration; it does not require modifying the system-wide linker configuration or disabling macOS protections.

> The setup script was subsequently revised to discover Homebrew's `libusb` pkg-config metadata automatically. **The revised script's `--check` result has not yet been provided**, so only the manual workaround and successful compilation are marked verified here.

### Build and run on macOS

From the repository root:

```bash
mkdir -p build
clang -std=c11 -Wall -Wextra -O2 \
    src/usb_device_info.c \
    -o build/usb_device_info \
    $(pkg-config --cflags --libs libusb-1.0)
./build/usb_device_info
```

**Observed program output (selected, no private identifiers):**

```text
====================================
     RTL8821CU USB DEVICE INSPECTOR
====================================

Vendor ID:  0x0bda
Product ID: 0x1a2b
USB Bus:    20
USB Device: 11
Mode:       CD-ROM / Installer

--- USB Configuration ---
Interfaces: 1

Interface 0 (Alt 0)
  Class: 0x08
  Endpoints: 2
    Endpoint: 0x8a
      Direction: IN
      Type: Bulk
      Max Packet Size: 512 bytes
    Endpoint: 0x0b
      Direction: OUT
      Type: Bulk
      Max Packet Size: 512 bytes
```

`0x08` is the USB Mass Storage class. The two bulk endpoints are part of the installation-mode interface. Bus and device numbers are temporary enumeration values, not permanent device identifiers.

### Linux vs. macOS comparison

| Check | Debian Linux | macOS Monterey |
| --- | --- | --- |
| C source compiled | Yes | Yes |
| `libusb` device enumeration | Yes | Yes |
| USB ID observed | `0bda:c820` | `0bda:1a2b` |
| Device mode | Wi-Fi operational mode | CD-ROM / installer mode |
| Interfaces reported by v0.2 | 3 | 1 |
| USB descriptors read | Yes | Yes |
| Wireless scanning | Verified with Linux driver | Not available / not tested |
| Wi-Fi connection and IP traffic | Not tested | Not tested |

**Conclusion:** The inspector's descriptor-reading functionality is now **verified on both platforms**. The antenna has **not** been switched to its operational Wi-Fi ID on macOS, and there is **no demonstrated native macOS Wi-Fi support**.

### Handoff to Phase 8

The next phase investigates device opening, interface ownership, and controlled Bulk-Only Transport (BOT) communication with `libusb`. USB re-enumeration, rather than a timeout or successful write, remains the criterion for a mode-switch result.

During Phase 7, no USB driver was detached, firmware changed, kernel extension installed, SIP disabled, or unrelated storage device ejected.

## Phase 8 — macOS USB access and BOT diagnostics

Phase 8 explores **userspace access to the Realtek installer-mode interface** and the behavior of USB Mass Storage **Bulk-Only Transport (BOT)** commands. Unlike the read-only inspector, the mode-switch experiment has an explicit write-capable mode. **No successful mode transition has been demonstrated on macOS.**

### Tools and source files

| File | Responsibility | USB changes |
| --- | --- | --- |
| `src/usb_device_info.c` | Enumerates the adapter and reads USB descriptors | Read-only |
| `src/usb_access_test.c` | Development experiment for open, driver-status check, claim, and release | Interface claim/release; no SCSI commands |
| `src/usb_mode_switch.c` | BOT protocol diagnostics (latest v5 source) | Default `--dry-run` is read-only; `--execute` sends **one** SCSI `0x1e` command and reads its CSW |

`usb_mode_switch.c` is currently a **diagnostic utility**, not a verified automatic mode-switcher. The v5 program does not send `START STOP UNIT / EJECT (0x1b)`, `REQUEST SENSE (0x03)`, or USB reset commands. No custom kernel extension or Wi-Fi driver has been installed by these programs.

### Device ownership and access validation (observed)

Initial `libusb` testing successfully opened and closed the Realtek device (`0bda:1a2b`). At first, `libusb_kernel_driver_active(handle, 0)` reported an active driver. An `ioreg` inspection identified an `IOUSBHostInterface@0` and USB user clients associated with third-party applications. After those applications were closed, the user clients were no longer visible and `libusb` reported no active kernel driver for interface 0.

The access test then succeeded in claiming and releasing interface 0:

```text
[OK] Realtek USB device opened successfully.
[INFO] No active kernel driver reported.
[TEST] Claiming USB interface 0...
[OK] Interface 0 claimed successfully.
[OK] Interface 0 released successfully.
[OK] Device handle closed.
```

This demonstrates successful **interface access for that session**. It does not prove that any particular third-party application was the sole cause of earlier driver ownership, or that the interface will always be available.

### Mass Storage interface and endpoints

The read-only preflight validated the exact installer-mode interface layout:

| Field | Validated value |
| --- | --- |
| VID:PID | `0bda:1a2b` |
| Interface | `0` |
| Class | `0x08` — Mass Storage |
| Subclass | `0x06` — SCSI transparent command set |
| Protocol | `0x50` — Bulk-Only Transport |
| Bulk OUT endpoint | `0x0b` |
| Bulk IN endpoint | `0x8a` |

In BOT, a **Command Block Wrapper (CBW)** is a 31-byte command envelope and a **Command Status Wrapper (CSW)** is a 13-byte response. The CSW includes a signature, a tag to correlate it with the CBW, a residue field, and a status byte.

### Experimental history — results from macOS

| Version | Experiment | Observed result |
| --- | --- | --- |
| v1 | Descriptor validation, `--dry-run` | Passed; no USB transfers |
| v2 | Claim interface, send SCSI `ALLOW MEDIUM REMOVAL (0x1e)` on LUN 0 | CBW transfer succeeded; received CSW status `1` (**Command Failed**); EJECT not sent |
| v3 | Attempt `REQUEST SENSE (0x03)` after command failure | A Bulk IN transfer completed with **13 of 18** requested bytes; the initial diagnostic did not interpret it |
| v4 | Dump and validate the 13-byte response | The returned bytes were a CSW, not SCSI sense data; the tag did not match the expected `REQUEST SENSE` tag |
| v5 | Single-command CBW/CSW tracing, no follow-up SCSI commands | **Source prepared; execution not yet verified** |

**Sanitized representative v4 result:**

```text
[OK] CBW sent: ALLOW MEDIUM REMOVAL (0x1e), LUN 0
[INFO] CSW: status=1, residue=0, tag=0x11223344
[INFO] SCSI command failed. Requesting diagnostic sense data...
[INFO] REQUEST SENSE IN: received 13 of 18 requested bytes
[RAW] 55 53 42 53 44 33 22 11 00 00 00 00 01
[WARN] Unexpected CSW tag; stopping.
[STOP] Diagnostic complete; EJECT not sent.
[INFO] Current USB enumeration: installer=1, wifi-mode=0
```

The 13 bytes decode as follows:

```text
55 53 42 53  | CSW signature ("USBS")
44 33 22 11  | Tag: 0x11223344 (little-endian)
00 00 00 00  | Residue: 0
01           | Status: 1 (Command Failed)
```

This is **consistent with a CSW appearing unexpectedly during the REQUEST SENSE data stage**; it is not enough to establish whether the cause was firmware behavior, a pending response, or a bug in the host implementation. A Bulk transfer completing successfully does **not** imply that the SCSI command succeeded.

### Current diagnostic v5 — build and usage

The current `src/usb_mode_switch.c` intentionally isolates **one** CBW/CSW transaction, prints the complete outgoing and incoming bytes, verifies the returned tag, and stops on unexpected responses. Its CLI options are `--dry-run` (default) and `--execute`.

Install/check dependencies using the existing setup script:

```bash
bash scripts/setup.sh --check
```

#### macOS compilation error: `libusb-1.0.pc` or `libusb.h` not found

If compiling `src/usb_mode_switch.c` produces messages such as:

```text
Package libusb-1.0 was not found in the pkg-config search path.
Perhaps you should add the directory containing `libusb-1.0.pc`
to the PKG_CONFIG_PATH environment variable
No package 'libusb-1.0' found
src/usb_mode_switch.c:13:10: fatal error: 'libusb.h' file not found
#include <libusb.h>
         ^~~~~~~~~~
1 error generated.
```

**First check that Homebrew provides `libusb`:**

```bash
brew list libusb
```

If it is not installed, run `brew install libusb pkgconf`. If Homebrew `libusb` is installed but the active `pkg-config` executable cannot find its `.pc` metadata (for example, when `pkg-config` comes from MacPorts), run **in the same Terminal session as the compiler**:

```bash
export PKG_CONFIG_PATH="$(brew --prefix libusb)/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
```

Verify package resolution:

```bash
pkg-config --modversion libusb-1.0
pkg-config --cflags --libs libusb-1.0
```

Then rerun the `clang` command below. The `libusb.h` header error may be a downstream consequence of the missing `pkg-config` flags, not evidence that the header is absent from Homebrew. This `export` applies to the current shell session; a standalone `scripts/setup.sh` process cannot automatically export variables back to its parent shell.

Compile from the repository root on macOS:

```bash
mkdir -p build
clang -std=c11 -Wall -Wextra -Wpedantic -O2 \
  src/usb_mode_switch.c \
  -o build/usb_mode_switch \
  $(pkg-config --cflags --libs libusb-1.0)
```

For Debian/Ubuntu, the same command can use `gcc` in place of `clang`.

Check the device without sending data:

```bash
./build/usb_device_info
./build/usb_mode_switch --dry-run
```

**Experimental only — sends a real command:** After verifying that exactly one adapter appears as `0bda:1a2b`, the Realtek virtual storage is not mounted or in use, and no other process is using the USB interface, run the following only in a controlled local test:

```bash
./build/usb_mode_switch --execute
```

The program requires the typed confirmation `DIAG 0bda:1a2b`. It sends exactly one SCSI `ALLOW MEDIUM REMOVAL (0x1e)` command to LUN 0, reads one CSW, and stops. This version **does not attempt a mode switch**. Do not repeat `--execute` following a transport error, and do not use `sudo`, driver detachment, or USB reset to force access.

### Findings and next steps

- **Verified:** `libusb` device opening; interface 0 claim/release; sending a SCSI CBW; obtaining CSW responses in earlier versions; macOS remains in installer mode.
- **Under investigation:** Why the `0x1e` command returns status `1` and why the earlier `REQUEST SENSE` data stage contained a CSW with the previous tag.
- **Not verified:** v5 execution results; a successful switch to `0bda:c820` on macOS; a native Wi-Fi network interface or connectivity on macOS.
- **Next test:** Compile v5, run its default dry-run, and collect the **single-command** CBW/CSW diagnostic before changing BOT sequencing or adding a mode-switch action.

Do not commit terminal logs containing application details, device serial numbers, network addresses, usernames, or unrelated USB device inventories.

## Reproducing the checks safely

The commands below inspect a **separate USB adapter** without unloading the kernel's Wi-Fi modules or changing the host's current network connection. Do not change the primary network interface while connected remotely over SSH.

1. Identify the Realtek adapter and its current mode:

   ```bash
   lsusb
   lsusb -t
   ```

2. Only if its **current** ID is `0bda:1a2b`, attempt switching and check the actual result:

   ```bash
   sudo usb_modeswitch -K -v 0bda -p 1a2b
   lsusb
   ```

3. If the ID is `0bda:c820`, inspect the driver and wireless interfaces:

   ```bash
   modinfo rtw88_8821cu
   lsusb -t
   lsmod | rg 'rtw88|rtw_'
   iw dev
   ```

4. Identify the correct **USB** wireless interface locally. Set a shell variable for the current terminal session (do not store its value in the repository):

   ```bash
   USB_WIFI_IFACE="$(for p in /sys/class/net/*; do
     [ "$(readlink -f "$p/device/driver")" = "/sys/bus/usb/drivers/rtw_8821cu" ] && basename "$p"
   done | head -n 1)"
   test -n "$USB_WIFI_IFACE" || { echo 'USB Wi-Fi interface not found'; false; }
   ```

5. Confirm the binding and scan **without displaying SSIDs or BSSIDs**:

   ```bash
   readlink -f "/sys/class/net/${USB_WIFI_IFACE}/device/driver"
   sudo iw dev "$USB_WIFI_IFACE" scan | rg 'freq:|signal:'
   ```

Never upload the full raw scan output or shell history to a public repository.

## Development roadmap

- [x] Identify installation-mode USB VID:PID (`0bda:1a2b`)
- [x] Confirm USB mass-storage presentation
- [x] Observe operational-mode USB VID:PID (`0bda:c820`)
- [x] Preserve before/after terminal evidence
- [x] Verify the `rtw88_8821cu` module and USB driver binding
- [x] Confirm `rtw88` module dependencies
- [x] Verify wireless scanning on 2.4 GHz and 5 GHz
- [ ] Verify Wi-Fi association and actual data transfer
- [ ] Isolate a repeatable USB mode-switch procedure
- [x] Build and run a read-only USB inspector v0.1 in C/`libusb` on Debian
- [x] Inspect USB configurations, interfaces, and endpoints (v0.2)
- [ ] Investigate the Linux kernel's `rtw88` driver architecture
- [x] Compile and run USB Device Inspector v0.2 on macOS Monterey
- [x] Identify the Realtek CD-ROM-mode interface and endpoints on macOS
- [ ] Verify the revised macOS setup script with `--check`
- [x] Open the Realtek installer-mode USB device from userspace on macOS
- [x] Claim and release installer interface 0 without detaching a driver
- [x] Inspect BOT CBW/CSW responses in controlled macOS experiments
- [ ] Run and evaluate the single-command BOT diagnostic v5
- [ ] Investigate and validate successful USB mode switching on macOS
- [ ] Research feasible USB tooling and networking support on macOS
- [x] Document manual build instructions for `usb_mode_switch.c`
- [ ] Add automated tests and reproducible build scripts

## Privacy and evidence policy

This repository is intended for **public sharing**. Only the technical data necessary to reproduce the work should be committed.

**Do not commit:**

- SSIDs, BSSIDs, real MAC addresses, Wi-Fi passwords, IP addresses, router administration details, or SSH keys.
- Hostnames, account names, absolute home-directory paths, device serial numbers, or raw diagnostic dumps.
- Screenshots of terminal windows containing identifying network details or unrelated device inventories.
- Complete `iw dev`, `iw ... scan`, `ip a`, `nmcli`, `journalctl`, or `dmesg` output without reviewing/redacting it first.

**Allowed in this technical record:** publicly documented USB IDs (`0bda:1a2b`, `0bda:c820`), kernel module names, USB interface classes, sanitized excerpts, non-identifying signal/frequency measurements, and read-only sample program output.

Compiled binaries in `build/` and unreviewed raw terminal captures must not be committed.

The two included initial screenshots contain no Wi-Fi SSIDs, router credentials, IP addresses, or MAC addresses visible in the provided images. They do, however, enumerate unrelated USB hardware. **Review them before publication** if even that hardware inventory should remain private; consider replacing the screenshots with carefully cropped evidence containing only the Realtek adapter lines.

## References

- [Linux Kernel Driver Database — RTL8821CU (`CONFIG_RTW88_8821CU`)](https://cateee.net/lkddb/web-lkddb/RTW88_8821CU.html)
- [Linux `rtw88` source code](https://github.com/torvalds/linux/tree/master/drivers/net/wireless/realtek/rtw88)
- [ALFA Network — RTL8821CU Linux documentation](https://docs.alfa.com.tw/Support/Linux/RTL8821CU/)
- [usb-modeswitch source code](https://github.com/NetworkManager/usb_modeswitch)
- [libusb API documentation](https://libusb.sourceforge.io/api-1.0/)
- [USB Mass Storage Bulk-Only Transport (USB-IF)](https://www.usb.org/document-library/mass-storage-bulk-only-transport-10)

---

*Research log: USB Inspector v0.2 and macOS interface-access tests verified; experimental BOT command/response findings documented through v4; v5 source prepared, not yet executed; macOS mode switching and Wi-Fi networking remain unverified.*
