# ESP32 port of NUT (ESP-IDF 6)

This overlay rebuilds the earlier `esp32-alpha` work against current
Network UPS Tools and ESP-IDF 6.x, without PlatformIO.

## Why this is not a copy of the old tree

The original port deleted most of NUT, moved the rest into `src/`, and
renamed `common.h` everywhere so ESP-IDF would compile it. That made
every NUT upgrade a rewrite.

This layout keeps upstream NUT in place:

- Firmware lives in `esp32/` and is built with `idf.py`
- CMake lists the NUT files to compile (no `GLOB_RECURSE`)
- Driver `main()` is renamed only at compile time (`-Dmain=drv_main`)
- `common.h` is resolved with `#include_next`, not a repo-wide rename
- `config.h` is a short ESP32 file, not a frozen autotools dump
- USB Host comes from the ESP Component Registry (`espressif/usb` and
  `espressif/usb_host_hid`), which is required on IDF 6
- AF_UNIX used by driver↔`upsd` is mapped to lwIP loopback **only**
  for `AF_UNIX`; TCP port 3493 is left alone
- Extra HID subdrivers are omitted with `NUT_ESP32_MINIMAL` so flash
  stays small, without deleting those sources

NUT patches are small and guarded with `ESP_PLATFORM` / `WITH_ESPUSB` /
`NUT_ESP32_MINIMAL`.

## Versions

| Piece | Original port | This overlay |
| --- | --- | --- |
| NUT | ~2.8.2-era commit `2dce981ef` | current `master` (post v2.8.5) |
| ESP-IDF | 5.4.0 | 6.0 or later (6.1 is current) |
| USB | in-tree `usb` component | `espressif/usb` + `usb_host_hid` |
| Build | PlatformIO or IDF | ESP-IDF `idf.py` only |

## Hardware

ESP32-S3 with USB-OTG (DevKitC-1, AtomS3, or equivalent) and a USB HID UPS
(APC HID was the original test target).

Waveshare ESP32-S3-ETH also brings up the onboard W5500. `upsd` listens
on every interface, so a client on the RJ45 reaches port 3493 after DHCP.
The softAP still starts from `wifi.conf`. The image is built for 8 MB
and fits in the low half of that board's 16 MB flash.

## Build

```bash
cd esp32
. $HOME/esp/esp-idf/export.sh   # Windows: export.bat from the IDF install
idf.py set-target esp32s3
idf.py build
idf.py -p COMx flash monitor
```

On Windows PowerShell, run the ESP-IDF export script from that install,
then the same `idf.py` commands.

Flash layout is `partitions.csv` (8 MB, two FATFS volumes: `/var` and
`/usr`).

## Runtime

- SoftAP SSID `nut` / password `espdonut`, from `/usr/local/etc/nut/wifi.conf`
- Captive portal on that AP (http://192.168.4.1/) edits `wifi.conf` and reboots
- NUT server on UDP/TCP port 3493, on Wi-Fi and on Ethernet when the W5500 links
- Config in `/usr/local/etc/nut/` (seeded if missing)
- Default users: `nut` / `espdonut` and `monuser` / `pass`

```bash
upsc hidapc@<esp32-ip>
```

See `ESP32_SECURITY.md` before putting this on a real network.

## Updating NUT later

1. `git fetch upstream && git merge upstream/master` (or rebase)
2. Resolve the few `ESP_PLATFORM` / `WITH_ESPUSB` / `NUT_ESP32_MINIMAL` hunks
3. Rebuild from `esp32/`
