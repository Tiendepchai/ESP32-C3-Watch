# ESP32-C3 ANCS Watch Monitor

Firmware production-style cho ESP32-C3 nhan notification tu iPhone qua BLE ANCS, luu trang thai bang NVS, va hien thi len OLED SH1106 I2C.

## Stack

- MCU: ESP32-C3
- Framework: ESP-IDF
- BLE host: NimBLE
- Protocol: Apple Notification Center Service (ANCS)
- Display: SH1106 128x64 qua I2C
- Display API: `esp_lcd` + custom SH1106 panel wrapper
- Storage: NVS

## Kien truc

- `main/`: app orchestration, state machine, routing event
- `components/ble_manager/`: GAP security, advertising, reconnect, NimBLE bootstrap
- `components/ancs_client/`: discover ANCS service/chars, subscribe notify, request/parse attributes
- `components/display_manager/`: I2C bus, SH1106 panel, framebuffer text rendering
- `components/notification_store/`: RAM notification queue/ring store
- `components/storage_manager/`: NVS config and bond flags
- `components/board_config/`: chan, kich thuoc man hinh, queue size, timeout

## Wiring

- OLED SH1106 I2C
- VCC -> 3V3
- GND -> GND
- SDA -> GPIO6
- SCL -> GPIO7
- Button A -> GPIO3, keo xuong GND khi nhan
- Button B -> GPIO4, keo xuong GND khi nhan

## Luu y quan trong ve topology ANCS

Project nay dung topology thuc te cua ANCS accessory:

- ESP32-C3 quang ba BLE de iPhone ket noi vao
- sau khi link duoc secure, ESP32-C3 dong vai GATT client de discover ANCS tren iPhone

Dieu nay khac voi mo hinh "ESP scan roi connect vao iPhone" nhung phu hop hon voi ANCS ngoai thuc te va bam theo vi du NimBLE ANCS cua ESP-IDF.

## Build

```bash
idf.py set-target esp32c3
idf.py build
idf.py flash monitor
```

## Default pinout

- SDA: GPIO6
- SCL: GPIO7
- SH1106 I2C address: `0x3C`
- Button A: GPIO3
- Button B: GPIO4

Sua tai `components/board_config/include/board_config.h`.

## Trang thai hien thi

- Booting...
- Waiting for iPhone
- Connected
- Notification received
- Disconnected

## Known limitations

- Can test ANCS that voi iPhone de xac nhan timing subscribe/discovery tren tung phien ban iOS.
- Parse Data Source hien tap trung vao `AppIdentifier`, `Title`, `Message`; chua xu ly day du moi attribute/action cua ANCS.
- Text renderer hien la font bitmap don gian 5x7, chua co auto-scroll dai dong.

## Next enhancements

- Nut `next` / `clear`
- Filter theo app/category
- Sleep mode
- Auto scroll text
- Icon app neu sau nay co iOS companion app
