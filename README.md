# ESP32-C3 ANCS Watch

## English

ESP32-C3 ANCS Watch is an ESP-IDF firmware project for a compact BLE watch-style accessory. It pairs with an iPhone, subscribes to Apple Notification Center Service (ANCS), fetches notification attributes, and renders notifications on a 128x64 SH1106 OLED.

The firmware is built around the ANCS accessory topology:

1. The ESP32-C3 advertises as a BLE peripheral.
2. The iPhone connects and bonds to the ESP32-C3.
3. After the link is encrypted, the ESP32-C3 acts as a GATT client and discovers ANCS on the iPhone.
4. The ESP32-C3 subscribes to ANCS Notification Source and Data Source.
5. For each notification UID, it requests `AppIdentifier`, `Title`, `Message`, and `Date`.
6. Notifications are stored in memory and sorted with the newest notification first.

### Current Features

- BLE pairing and bonding with NimBLE.
- Bonded reconnect flow with directed advertising first, then general advertising fallback.
- ANCS service discovery, CCCD subscription, and attribute fetching.
- Date-based notification ordering using ANCS `Date` when available.
- Current Time Service read for watchface time sync.
- SH1106 OLED rendering for status, watchface, notifications, and navigation views.
- Local notification store with configurable filtering.
- NVS-backed app settings and bond state.
- Two-button control for navigation, clearing notifications, filter changes, and bond reset.
- Optional iOS SwiftUI companion scaffold for configuration and navigation handoff.

### Hardware

- MCU: ESP32-C3
- Display: SH1106 128x64 OLED over I2C
- Button A: GPIO3, active low
- Button B: GPIO4, active low

Default wiring:

| Function | ESP32-C3 Pin |
| --- | --- |
| OLED VCC | 3V3 |
| OLED GND | GND |
| OLED SDA | GPIO6 |
| OLED SCL | GPIO7 |
| Button A | GPIO3 to GND |
| Button B | GPIO4 to GND |

Board constants live in `components/board_config/include/board_config.h`.

### Firmware Structure

| Path | Purpose |
| --- | --- |
| `main/` | Application state machine and UI/event orchestration |
| `components/ble_manager/` | NimBLE setup, advertising, bonding, reconnect, config GATT service |
| `components/ancs_client/` | ANCS discovery, notification subscription, attribute request/parse |
| `components/cts_client/` | iOS Current Time Service client |
| `components/display_manager/` | SH1106 display driver, framebuffer, notification/watchface rendering |
| `components/notification_store/` | In-memory notification storage and sorting |
| `components/storage_manager/` | NVS-backed settings |
| `components/power_manager/` | Optional battery/power monitoring |
| `ios/ANCSWatchConfig/` | SwiftUI companion app scaffold |

### Notification Ordering

The ordering pipeline is:

1. Receive ANCS Notification Source.
2. Extract `NotificationUID`.
3. Send `GetNotificationAttributes(NotificationUID, AppIdentifier, Title, Message, Date)`.
4. Parse Data Source response.
5. Use ANCS `Date` as the notification timestamp when present.
6. Store and sort notifications by timestamp descending.

Before attributes are fetched, the firmware uses the local receive time as a temporary timestamp. Once `Date` arrives, the stored record is updated and the list is re-sorted.

### Build and Flash

Activate ESP-IDF v6.0:

```bash
source /Users/allah-computer/.espressif/tools/activate_idf_v6.0.sh
```

Build:

```bash
idf.py set-target esp32c3
idf.py build
```

Flash and monitor:

```bash
idf.py -p /dev/cu.usbmodem113301 flash monitor
```

If the local Python environment hits the macOS `platform.mac_ver()` issue, the known workaround used during development is:

```bash
IDF_COMPONENT_MANAGER=0 python -c 'import os, platform, runpy, sys; tools=os.path.join(os.environ["IDF_PATH"],"tools"); sys.path.insert(0,tools); platform.mac_ver=lambda release="", versioninfo=("", "", ""), machine="": ("26.1", ("", "", ""), os.uname().machine); sys.argv=[os.path.join(tools,"idf.py")]+sys.argv[1:]; runpy.run_path(sys.argv[0], run_name="__main__")' build
```

### Test Checklist

- Clear iOS Bluetooth bond and ESP32-C3 bonds before testing a first-pair flow.
- Pair from iOS or nRF Connect and verify the first bond does not remove the local bond.
- Confirm logs show `bond store peers after secure=1`.
- Wait for `ANCS ready, attrs=enabled`.
- Send several notifications, then disconnect by walking out of range.
- Reconnect and verify pre-existing notifications wake the display and appear.
- Verify fetched notification details reorder the list by ANCS `Date`.

### License

This project is licensed under the MIT License. See `LICENSE`.

---

## Tiếng Việt

ESP32-C3 ANCS Watch là firmware ESP-IDF cho một thiết bị đeo nhỏ dùng BLE. Thiết bị ghép đôi với iPhone, đăng ký Apple Notification Center Service (ANCS), lấy chi tiết thông báo và hiển thị lên màn hình OLED SH1106 128x64.

Firmware dùng đúng mô hình accessory của ANCS:

1. ESP32-C3 quảng bá BLE như một peripheral.
2. iPhone kết nối và tạo bond với ESP32-C3.
3. Sau khi link đã mã hóa, ESP32-C3 đóng vai trò GATT client và discover ANCS trên iPhone.
4. ESP32-C3 subscribe ANCS Notification Source và Data Source.
5. Với mỗi notification UID, firmware request `AppIdentifier`, `Title`, `Message` và `Date`.
6. Thông báo được lưu trong RAM và sắp xếp để thông báo mới nhất nằm đầu danh sách.

### Tính năng hiện có

- Pairing và bonding BLE bằng NimBLE.
- Reconnect theo bond cũ: directed advertising trước, sau đó fallback sang general advertising.
- Discover ANCS, subscribe CCCD và fetch attribute.
- Sắp xếp thông báo theo thời gian thật từ ANCS `Date` khi có.
- Đồng bộ giờ watchface qua iOS Current Time Service.
- Hiển thị status, watchface, thông báo và điều hướng trên OLED SH1106.
- Store thông báo trong RAM, có filter theo nhóm/app.
- Lưu cấu hình và trạng thái bond bằng NVS.
- Hai nút bấm để chuyển thông báo, xóa thông báo, đổi filter và reset bond.
- Có scaffold app iOS SwiftUI để cấu hình và gửi trạng thái điều hướng.

### Phần cứng

- MCU: ESP32-C3
- Màn hình: OLED SH1106 128x64 qua I2C
- Nút A: GPIO3, kéo xuống GND khi nhấn
- Nút B: GPIO4, kéo xuống GND khi nhấn

Sơ đồ chân mặc định:

| Chức năng | Chân ESP32-C3 |
| --- | --- |
| OLED VCC | 3V3 |
| OLED GND | GND |
| OLED SDA | GPIO6 |
| OLED SCL | GPIO7 |
| Nút A | GPIO3 xuống GND |
| Nút B | GPIO4 xuống GND |

Các hằng số phần cứng nằm trong `components/board_config/include/board_config.h`.

### Cấu trúc firmware

| Đường dẫn | Vai trò |
| --- | --- |
| `main/` | State machine của app và điều phối event/UI |
| `components/ble_manager/` | Khởi tạo NimBLE, advertising, bonding, reconnect, config GATT service |
| `components/ancs_client/` | Discover ANCS, subscribe thông báo, request/parse attribute |
| `components/cts_client/` | Client đọc giờ từ iOS Current Time Service |
| `components/display_manager/` | Driver SH1106, framebuffer, render watchface/thông báo |
| `components/notification_store/` | Lưu và sắp xếp thông báo trong RAM |
| `components/storage_manager/` | Lưu cấu hình bằng NVS |
| `components/power_manager/` | Theo dõi pin/nguồn, hiện đang tùy chọn |
| `ios/ANCSWatchConfig/` | Scaffold app iOS SwiftUI |

### Thứ tự thông báo

Luồng sắp xếp hiện tại là:

1. ESP32 nhận ANCS Notification Source.
2. Lấy `NotificationUID`.
3. Gửi `GetNotificationAttributes(NotificationUID, AppIdentifier, Title, Message, Date)`.
4. Parse phản hồi từ Data Source.
5. Nếu có ANCS `Date`, dùng giá trị đó làm timestamp của thông báo.
6. Lưu vào store và sort giảm dần theo timestamp, tức thông báo mới nhất nằm đầu.

Trước khi fetch chi tiết xong, firmware dùng tạm thời điểm ESP32 nhận event. Khi `Date` từ iPhone trả về, record được cập nhật timestamp và danh sách được sắp xếp lại.

### Build và flash

Kích hoạt ESP-IDF v6.0:

```bash
source /Users/allah-computer/.espressif/tools/activate_idf_v6.0.sh
```

Build:

```bash
idf.py set-target esp32c3
idf.py build
```

Flash và mở monitor:

```bash
idf.py -p /dev/cu.usbmodem113301 flash monitor
```

Nếu môi trường Python trên macOS gặp lỗi `platform.mac_ver()`, workaround đã dùng trong lúc phát triển là:

```bash
IDF_COMPONENT_MANAGER=0 python -c 'import os, platform, runpy, sys; tools=os.path.join(os.environ["IDF_PATH"],"tools"); sys.path.insert(0,tools); platform.mac_ver=lambda release="", versioninfo=("", "", ""), machine="": ("26.1", ("", "", ""), os.uname().machine); sys.argv=[os.path.join(tools,"idf.py")]+sys.argv[1:]; runpy.run_path(sys.argv[0], run_name="__main__")' build
```

### Checklist kiểm thử

- Xóa bond Bluetooth trên iOS và xóa bond trên ESP32-C3 trước khi test luồng pair lần đầu.
- Pair từ iOS hoặc nRF Connect và xác nhận bond đầu tiên không làm mất bond local.
- Kiểm tra log có `bond store peers after secure=1`.
- Chờ log `ANCS ready, attrs=enabled`.
- Gửi vài thông báo, sau đó đi ra xa để mất kết nối.
- Quay lại gần thiết bị và kiểm tra thông báo pre-existing có bật màn hình và hiển thị.
- Kiểm tra sau khi fetch chi tiết, danh sách thông báo được sắp xếp theo ANCS `Date`.

### License

Dự án dùng giấy phép MIT. Xem file `LICENSE`.
