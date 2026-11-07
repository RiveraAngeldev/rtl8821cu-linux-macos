# RTL8821CU — Linux & macOS USB Wi-Fi Research

> A hands-on systems programming project exploring USB device enumeration, Realtek Wi-Fi adapters, mode switching, and the feasibility of Linux/macOS driver development.

**Project stage:** Hardware identification and USB mode-switching research  
**Current result:** Adapter re-enumerated from `0bda:1a2b` (virtual CD-ROM) to `0bda:c820` (802.11ac NIC)  
**Driver status:** **Not yet verified** — Wi-Fi connectivity has not been demonstrated.

## Table of contents

- [Project overview](#project-overview)
- [Test environment and hardware](#test-environment-and-hardware)
- [Initial investigation: CD-ROM mode](#initial-investigation-cd-rom-mode)
- [USB mode-switching experiment](#usb-mode-switching-experiment)
- [Result: Wi-Fi mode detected](#result-wi-fi-mode-detected)
- [Commands and tools explained](#commands-and-tools-explained)
- [Reproducing the investigation](#reproducing-the-investigation)
- [Current limitations](#current-limitations)
- [Development roadmap](#development-roadmap)
- [References](#references)

## Project overview

Some USB Wi-Fi adapters initially identify themselves as a **virtual CD-ROM / mass-storage device** containing installation software. Until the device changes mode, the operating system cannot use it as a Wi-Fi adapter.

This repository records the investigation of one Realtek USB adapter on Debian Linux. The longer-term goal is to understand its USB behavior, develop portable diagnostic utilities in **C with `libusb`**, study the relevant **Linux kernel driver**, and evaluate what can realistically be implemented on **macOS**.

**Scope:** Learning and experimentation. This is **not currently a functioning custom driver** or a macOS Wi-Fi driver.

## Test environment and hardware

| Item | Observed detail |
| --- | --- |
| Host | `ANUBIS` — Debian Linux homelab |
| USB vendor | Realtek Semiconductor Corp. |
| Initial USB ID | `0bda:1a2b` |
| Initial USB description | `RTL8188GU 802.11n WLAN Adapter (Driver CDROM Mode)` |
| Post-switch USB ID | `0bda:c820` |
| Post-switch USB description | `802.11ac NIC` |
| Probable chipset / driver family | **RTL8821CU / Linux `rtw88_8821cu`**, based on post-switch USB ID; driver binding on this system still unverified |
| `usb_modeswitch` version | `2.6.1` (from terminal output) |

> **Identification note:** The initial `RTL8188GU` string is the USB ID database's description of **installation mode**, not conclusive proof of the underlying Wi-Fi chipset. The post-switch ID `0bda:c820` is listed by Linux driver databases as supported by `rtw88_8821cu`.

## Initial investigation: CD-ROM mode

### 1. List connected USB devices

```bash
lsusb
```

`lsusb` lists devices visible to Linux on the USB buses. Each entry includes a **Vendor ID (VID)** and **Product ID (PID)**, which are critical for matching hardware to drivers and USB mode-switching rules.

The adapter initially appeared as:

```text
Bus 001 Device 004: ID 0bda:1a2b Realtek Semiconductor Corp. RTL8188GU 802.11n WLAN Adapter (Driver CDROM Mode)
```

**Interpretation:** The device was visible to the host, but identified itself using a USB storage/install-mode ID rather than its Wi-Fi operational ID.

### 2. Inspect the USB device tree

```bash
lsusb -t
```

This displays the USB topology, interface classes, negotiated speeds, and currently bound drivers.

The adapter's initial entry was:

```text
|__ Port 001: Dev 004, If 0, Class=Mass Storage, Driver=[none], 480M
```

**Interpretation:** Interface 0 identified as **Mass Storage**. `Driver=[none]` shows that no kernel driver was bound to that interface at the time of inspection.

### 3. Inspect kernel logs

```bash
sudo journalctl -k -b --no-pager | tail -60
```

Relevant log messages included:

```text
usb 1-1: New USB device found, idVendor=0bda, idProduct=1a2b
usb 1-1: Product: DISK
usb 1-1: Manufacturer: Realtek
usb-storage 1-1:1.0: USB Mass Storage device detected
usb-storage 1-1:1.0: device ignored
```

These messages reinforced the initial diagnosis: Linux enumerated the adapter as a virtual storage device.

### Evidence — before the mode change

![Figure 1 — Realtek adapter in CD-ROM mode, including the first USB mode-switch attempt and USB tree](docs/images/01-initial-cdrom-mode.png)

*Figure 1. The initial `usb_modeswitch`, `lsusb`, and `lsusb -t` output. The adapter still appears as `0bda:1a2b` with `Class=Mass Storage` after this particular attempt.*

## USB mode-switching experiment

The `usb-modeswitch` utility can send device-specific commands to USB devices that present more than one operational mode.

The first attempted command was:

```bash
sudo usb_modeswitch -K -v 0bda -p 1a2b
```

| Argument | Meaning |
| --- | --- |
| `-K` | Send the standard USB storage **eject** sequence |
| `-v 0bda` | Target USB vendor ID (`Realtek`) |
| `-p 1a2b` | Target product ID in the initial mode |

The tool reported:

```text
Sending standard EJECT sequence
Sending the message returned error -7. Try to continue
Response reading failed (error -7)
Device is gone, skip any further commands
```

In the underlying `libusb` error codes, `-7` represents a **timeout**. The message saying the device is gone suggests that the previous USB device handle was no longer usable, but **does not by itself prove a successful switch**.

Indeed, the next `lsusb` observation **still showed `0bda:1a2b`**. We therefore recorded this first attempt as **inconclusive / not yet successful**.

Later in the investigation, the device was observed with a new USB ID: **`0bda:c820`**. The exact event that triggered the eventual transition was not isolated in the available logs, so this repository does **not** claim that the first command, on its own, caused the successful change.

## Result: Wi-Fi mode detected

A subsequent `lsusb` check showed:

```text
Bus 001 Device 006: ID 0bda:c820 Realtek Semiconductor Corp. 802.11ac NIC
```

This is the key result: the USB device was now enumerated with its **wireless operational ID** rather than its installation-mode ID.

A follow-up diagnostic command was executed:

```bash
sudo usb_modeswitch -v 0bda -p 1a2b -KW
```

Here, `-W` enables verbose output. Because the adapter was **already identified as `0bda:c820` before this command**, the tool correctly responded:

```text
No devices in default mode found. Nothing to do. Bye!
```

**Important:** This later `-KW` command did **not** perform the successful switch; it confirmed that no `0bda:1a2b` device remained available for switching.

### Evidence — after the mode change

![Figure 2 — Realtek adapter enumerated in 802.11ac Wi-Fi mode](docs/images/02-wifi-mode-detected.png)

*Figure 2. `lsusb` now identifies the adapter as `0bda:c820` (`802.11ac NIC`). The verbose `usb_modeswitch` attempt below it finds no device still in the original CD-ROM mode.*

### Summary of observed USB state changes

| Stage | USB ID | OS description | What we know |
| --- | --- | --- | --- |
| Initial detection | `0bda:1a2b` | Driver CD-ROM Mode | Storage-mode USB interface observed |
| After first eject attempt | `0bda:1a2b` | Driver CD-ROM Mode | Timeout was reported; initial mode still visible |
| Later observation | `0bda:c820` | 802.11ac NIC | Device re-enumerated in wireless mode |
| Subsequent verbose test | `0bda:c820` | 802.11ac NIC | No installation-mode device remained to switch |

## Commands and tools explained

| Tool / command | Purpose in the investigation | Status |
| --- | --- | --- |
| `lsusb` | Display current USB devices and their VID:PID values | Used successfully |
| `lsusb -t` | Inspect interface class, USB topology, and bound driver | Used successfully |
| `journalctl -k -b` | Examine current-boot kernel messages related to USB enumeration | Used successfully |
| `usb_modeswitch` | Attempt to change USB operating mode | Used; later Wi-Fi-mode ID observed |
| `iw dev` | Display wireless PHY/interfaces, once available | Was initially unavailable (`command not found`); not yet verified |
| `modinfo rtw88_8821cu` | Inspect information about the potential Linux kernel module | Not executed successfully (`modinfo: command not found`) |

### Package setup

The setup commands discussed for Debian were:

```bash
sudo apt update
sudo apt install usb-modeswitch usb-modeswitch-data iw
```

- **`usb-modeswitch`** — sends the USB switching commands; installation/execution is confirmed by the captured output.
- **`usb-modeswitch-data`** — provides configurations for supported mode-switching devices; installation on this host has not been independently verified.
- **`iw`** — inspects and configures Linux Wi-Fi interfaces; the first attempt to use it returned `command not found`.
- **`usbutils`** — package that typically provides `lsusb` on Debian; the executable was available, but how it was installed was not recorded.
- **`kmod`** — normally provides `modinfo`; currently missing from the shell's command path.

> We distinguish between **commands suggested** and **packages confirmed installed** to keep the project log reproducible.

## Reproducing the investigation

**Safety:** On a remote host, avoid removing the driver for the network interface carrying your SSH session. These diagnostic steps do not intentionally change the primary network interface.

1. Connect the USB adapter directly to the Linux machine.
2. Record the initial USB ID and topology:

   ```bash
   lsusb
   lsusb -t
   ```

3. If the device is **still** in installation mode (`0bda:1a2b`), test the standard eject sequence:

   ```bash
   sudo usb_modeswitch -K -v 0bda -p 1a2b
   ```

4. Check the actual device state afterwards:

   ```bash
   lsusb
   lsusb -t
   ```

5. If it appears as `0bda:c820`, proceed to driver diagnostics rather than repeating the switch command.

For future kernel driver investigation, the next **proposed** steps are:

```bash
sudo apt install kmod iw
modinfo rtw88_8821cu
lsusb -t
iw dev
```

These are **next steps**, not completed tests. Driver module availability, binding, firmware loading, and working Wi-Fi operation must still be established.

## Current limitations

- **USB Wi-Fi mode is detected**, but successful network operation has **not** yet been confirmed.
- The `0bda:c820` ID is compatible with the RTL8821CU driver family; a working driver binding on **this exact machine** remains unverified.
- The precise trigger/time of the successful mode change has not been conclusively isolated.
- The adapter is advertised as Wi-Fi/Bluetooth, but only the **Wi-Fi USB mode** has been investigated. Bluetooth support is out of scope for now.
- **macOS support is a research objective, not an existing feature.** A portable userspace USB diagnostic utility is a more realistic first milestone than promising a native macOS Wi-Fi driver.
- No adapter firmware was read, replaced, or modified during this experiment.

## Development roadmap

- [x] Detect the USB adapter in Linux
- [x] Record the storage-mode VID:PID (`0bda:1a2b`)
- [x] Inspect its original USB interface class
- [x] Attempt standard-eject mode switching and record the result
- [x] Observe the operational VID:PID (`0bda:c820`)
- [x] Capture before/after terminal evidence
- [ ] Confirm the Linux kernel module is present and bound to this adapter
- [ ] Confirm `iw dev` reports the new Wi-Fi interface
- [ ] Determine the sequence that reliably triggers the USB mode change
- [ ] Build a read-only cross-platform USB enumerator in C/`libusb`
- [ ] Implement a carefully tested userspace mode-switching experiment
- [ ] Study Linux `rtw88` USB code and document the relevant execution path
- [ ] Evaluate macOS USB support and networking limitations
- [ ] Add automated tests and reproducible build instructions

## References

- [Linux Kernel Driver Database — RTL8821CU (`CONFIG_RTW88_8821CU`)](https://cateee.net/lkddb/web-lkddb/RTW88_8821CU.html)
- [Linux Kernel Driver Database — Realtek USB IDs including `0bda:c820`](https://cateee.net/lkddb/web-lkddb/RTW88.html)
- [ALFA Network — RTL8821CU Linux support information](https://docs.alfa.com.tw/Support/Linux/RTL8821CU/)
- [Linux kernel source — `drivers/net/wireless/realtek/rtw88`](https://github.com/torvalds/linux/tree/master/drivers/net/wireless/realtek/rtw88)
- [usb-modeswitch source](https://github.com/NetworkManager/usb_modeswitch)

---

**Repository purpose:** A transparent lab notebook and starting point for real, incremental USB / Linux systems programming work. Findings are marked as observed, inferred, or pending verification.
