# ESP32-C3 ANCS Watch

[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v6.0+-blue.svg)](https://github.com/espressif/esp-idf)
[![Target](https://img.shields.io/badge/Target-ESP32--C3-green.svg)](https://www.espressif.com/en/products/socs/esp32-c3)
[![BLE](https://img.shields.io/badge/BLE-ANCS-informational.svg)](https://developer.apple.com/library/archive/documentation/CoreBluetooth/Reference/AppleNotificationCenterServiceSpecification/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

**ESP32-C3 ANCS Watch** is an ESP-IDF firmware project for a compact BLE watch-style accessory.  
It pairs with an iPhone, subscribes to Apple Notification Center Service (ANCS), fetches notification attributes, synchronizes time, and renders notifications on a 128 × 64 SH1106 OLED display.

> **Project status:** Active firmware prototype. The core BLE/ANCS/display flow is implemented; power monitoring and iOS companion features are optional or under development.

---

## Table of Contents

- [Overview](#overview)
- [Key Features](#key-features)
- [System Architecture](#system-architecture)
- [Hardware](#hardware)
- [Firmware Layout](#firmware-layout)
- [Notification Ordering](#notification-ordering)
- [Getting Started](#getting-started)
- [Build and Flash](#build-and-flash)
- [Usage](#usage)
- [Testing Checklist](#testing-checklist)
- [Troubleshooting](#troubleshooting)
- [Roadmap](#roadmap)
- [License](#license)

---

## Overview

The firmware implements the standard ANCS accessory flow:

1. The ESP32-C3 advertises as a BLE peripheral.
2. The iPhone connects and creates a secure BLE bond.
3. After encryption is enabled, the ESP32-C3 switches role logically to a GATT client and discovers ANCS on the iPhone.
4. The ESP32-C3 subscribes to the ANCS Notification Source and Data Source characteristics.
5. For each notification UID, the firmware requests:
   - `AppIdentifier`
   - `Title`
   - `Message`
   - `Date`
6. Notifications are stored locally and sorted with the newest notification first.
7. The display manager renders connection status, watchface, notification detail, and navigation views.

---

## Key Features

- BLE pairing and bonding powered by NimBLE.
- Bonded reconnect flow using directed advertising first, then general advertising fallback.
- ANCS discovery, CCCD subscription, notification event handling, and attribute parsing.
- Date-based notification ordering using ANCS `Date` when available.
- Temporary local receive timestamp while waiting for notification attributes.
- iOS Current Time Service client for watchface time synchronization.
- SH1106 128 × 64 OLED rendering for:
  - connection status
  - watchface
  - notification list/detail
  - navigation state
- In-memory notification store with configurable filtering.
- NVS-backed settings and bond state persistence.
- Two-button physical control for:
  - notification navigation
  - clearing notifications
  - changing filters
  - resetting bond state
- Optional SwiftUI companion scaffold for future configuration and navigation handoff.

---

## System Architecture

```text
┌────────────────────┐
│      iPhone         │
│  ANCS + CTS Server  │
└─────────┬──────────┘
          │ BLE encrypted link
          ▼
┌──────────────────────────────────────┐
│              ESP32-C3                │
│                                      │
│  ┌───────────────┐   ┌────────────┐  │
│  │ BLE Manager   │──▶│ ANCS Client│  │
│  └───────┬───────┘   └─────┬──────┘  │
│          │                 │         │
│          ▼                 ▼         │
│  ┌───────────────┐   ┌────────────┐  │
│  │ CTS Client    │   │ Notification│ │
│  │ Time Sync     │   │ Store       │ │
│  └───────┬───────┘   └─────┬──────┘  │
│          │                 │         │
│          ▼                 ▼         │
│  ┌────────────────────────────────┐  │
│  │        Display Manager          │  │
│  │      SH1106 OLED Rendering      │  │
│  └────────────────────────────────┘  │
│                                      │
│  ┌────────────────────────────────┐  │
│  │        Storage Manager          │  │
│  │        NVS Settings/Bonds       │  │
│  └────────────────────────────────┘  │
└──────────────────────────────────────┘
```

---

## Hardware

| Component | Specification |
| --- | --- |
| MCU | ESP32-C3 |
| Display | SH1106 128 × 64 OLED |
| Display bus | I2C |
| Button A | GPIO3, active low |
| Button B | GPIO4, active low |

### Default Wiring

| Function | ESP32-C3 Pin |
| --- | --- |
| OLED VCC | 3V3 |
| OLED GND | GND |
| OLED SDA | GPIO6 |
| OLED SCL | GPIO7 |
| Button A | GPIO3 to GND |
| Button B | GPIO4 to GND |

Board-level constants are defined in:

```text
components/board_config/include/board_config.h
```

---

## Firmware Layout

| Path | Purpose |
| --- | --- |
| `main/` | Application state machine, UI flow, and event orchestration |
| `components/ble_manager/` | NimBLE initialization, advertising, bonding, reconnect, and config GATT service |
| `components/ancs_client/` | ANCS discovery, notification subscription, attribute request, and response parser |
| `components/cts_client/` | iOS Current Time Service client |
| `components/display_manager/` | SH1106 driver, framebuffer, watchface rendering, and notification rendering |
| `components/notification_store/` | In-memory notification storage, filtering, and sorting |
| `components/storage_manager/` | NVS-backed settings and persistent state |
| `components/power_manager/` | Optional battery and power monitoring |
| `ios/ANCSWatchConfig/` | Optional SwiftUI companion app scaffold |

---

## Notification Ordering

The notification ordering pipeline is designed to avoid relying only on local receive time.

```text
ANCS Notification Source event
        │
        ▼
Extract NotificationUID
        │
        ▼
Request attributes:
AppIdentifier, Title, Message, Date
        │
        ▼
Parse ANCS Data Source response
        │
        ▼
Use ANCS Date if available
        │
        ▼
Update notification record
        │
        ▼
Sort newest first
```

When a notification event first arrives, the firmware stores it with the local receive timestamp as a temporary value.  
After the ANCS `Date` attribute is fetched, the stored record is updated and the notification list is re-sorted by timestamp in descending order.

This allows notifications such as:

```text
Messenger  - 1 second ago
MoMo       - 2 minutes ago
```

to appear in the expected order, with the newest notification at the top.

---

## Getting Started

### Prerequisites

Install and configure:

- ESP-IDF v6.0 or newer
- Python environment required by ESP-IDF
- USB serial driver for your ESP32-C3 board
- An iPhone with Bluetooth enabled
- Optional: nRF Connect for BLE debugging

### Clone the Repository

```bash
git clone <your-repository-url>
cd esp32-c3-ancs-watch
```

### Set the ESP-IDF Target

```bash
idf.py set-target esp32c3
```

---

## Build and Flash

### Activate ESP-IDF

Use the activation script generated by your ESP-IDF installation.

Example:

```bash
source "$HOME/.espressif/tools/activate_idf_v6.0.sh"
```

Alternatively, if ESP-IDF is installed globally:

```bash
. "$IDF_PATH/export.sh"
```

### Build

```bash
idf.py build
```

### Flash

Replace the port with the serial device used by your board.

```bash
idf.py -p /dev/cu.usbmodemXXXX flash
```

### Monitor Logs

```bash
idf.py -p /dev/cu.usbmodemXXXX monitor
```

### Flash and Monitor in One Command

```bash
idf.py -p /dev/cu.usbmodemXXXX flash monitor
```

---

## Usage

### First Pairing

1. Flash the firmware to the ESP32-C3.
2. Open the Bluetooth settings on iOS.
3. Select the ESP32-C3 accessory.
4. Accept the pairing request.
5. Wait until the BLE link is encrypted and ANCS discovery completes.
6. Send a test notification to the iPhone.
7. Verify that the OLED displays the notification.

### Reconnect Flow

After a successful bond, the firmware tries:

1. Directed advertising to the bonded iPhone.
2. General advertising fallback if directed reconnect fails.
3. ANCS rediscovery after the encrypted link is restored.

### Button Controls

| Action | Expected Behavior |
| --- | --- |
| Button A | Navigate through notifications or UI views |
| Button B | Change filter, clear notification, or trigger secondary action |
| Long press / configured combo | Reset bond state or enter maintenance flow |

> Exact button behavior depends on the current firmware state machine and board configuration.

---

## Testing Checklist

Before testing a first-pair flow:

- Clear the iOS Bluetooth bond for the ESP32-C3.
- Clear stored bonds on the ESP32-C3.
- Reboot both devices if bonding behavior is inconsistent.

Recommended validation sequence:

- Pair from iOS or nRF Connect.
- Confirm the first bond is retained locally.
- Check logs for:

```text
bond store peers after secure=1
```

- Wait for:

```text
ANCS ready, attrs=enabled
```

- Send several notifications from different apps.
- Verify that notification details are fetched correctly.
- Verify that notifications are sorted by ANCS `Date`.
- Walk out of range to force a disconnect.
- Return to range and confirm bonded reconnect works.
- Confirm pre-existing or pending notifications can wake the display and render correctly.
- Test button navigation, filter changes, clear action, and bond reset.

---

## Troubleshooting

### ANCS does not become ready

Check that:

- The iPhone accepted the BLE pairing request.
- The connection is encrypted.
- The ESP32-C3 bond was not erased after pairing.
- ANCS discovery runs only after encryption.
- Notification permissions are enabled on iOS.

Useful log marker:

```text
ANCS ready, attrs=enabled
```

### Notifications appear but details are missing

Check that:

- Data Source subscription succeeded.
- Control Point writes are accepted.
- `GetNotificationAttributes` is sent with the correct `NotificationUID`.
- The parser handles fragmented Data Source responses.

### Notification order is incorrect

Check that:

- `Date` is included in the requested ANCS attributes.
- The firmware updates the temporary receive timestamp after `Date` arrives.
- The notification store re-sorts the list after attribute parsing.

### Reconnect does not work

Check that:

- Bond data is stored in NVS.
- Directed advertising uses the bonded peer address.
- General advertising fallback starts after directed advertising fails.
- iOS still has the accessory saved in Bluetooth settings.

### macOS ESP-IDF `platform.mac_ver()` issue

If the local Python environment fails because of a macOS version parsing issue, use a clean ESP-IDF Python environment first.  
If the issue persists, the following development workaround can be used:

```bash
IDF_COMPONENT_MANAGER=0 python -c 'import os, platform, runpy, sys; tools=os.path.join(os.environ["IDF_PATH"],"tools"); sys.path.insert(0,tools); platform.mac_ver=lambda release="", versioninfo=("", "", ""), machine="": ("26.1", ("", "", ""), os.uname().machine); sys.argv=[os.path.join(tools,"idf.py")]+sys.argv[1:]; runpy.run_path(sys.argv[0], run_name="__main__")' build
```

---

## Roadmap

- [ ] Add battery percentage monitoring.
- [ ] Add charging status display.
- [ ] Improve notification grouping by app.
- [ ] Add persistent notification history if required.
- [ ] Finalize iOS SwiftUI companion app.
- [ ] Add configurable button actions.
- [ ] Add screenshots or hardware photos.
- [ ] Add CI build workflow for ESP-IDF.
- [ ] Add release binaries and flashing instructions.

---

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.

---

## Vietnamese Summary

**ESP32-C3 ANCS Watch** là firmware ESP-IDF cho thiết bị đeo nhỏ dùng BLE. Thiết bị ghép đôi với iPhone, subscribe Apple Notification Center Service, lấy thông tin `AppIdentifier`, `Title`, `Message`, `Date`, sau đó hiển thị thông báo lên OLED SH1106 128 × 64.

Điểm chính:

- Pairing/bonding BLE bằng NimBLE.
- Tự reconnect với iPhone đã bond.
- Fetch chi tiết thông báo qua ANCS.
- Sort thông báo theo `Date` để thông báo mới nhất hiện trên đầu.
- Đồng bộ giờ qua iOS Current Time Service.
- Hiển thị watchface và notification bằng SH1106 OLED.
- Lưu cấu hình bằng NVS.
- Điều khiển bằng hai nút vật lý.
