# Firmware Migration Notes: Super Mini → ESP32-C3-MINI-1-N4

**Date:** 2026-06-16

---

## Key Changes Required

### 1. USB Serial/JTAG Console
- Change from UART console to USB-SERIAL-JTAG
- `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` in sdkconfig
- UART GPIO20/21 retained as test pads for fallback only

### 2. Flash Size & Partition Table
- Change from 2MB to 4MB (MINI-1-N4 has 4MB flash)
- `CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y`
- `CONFIG_PARTITION_TABLE_TWO_OTA=y` recommended

### 3. Battery ADC
- Change divider: 220k/100k → 470k/470k (ratio changes 0.3125 → 0.5)
- Update `BAT_ADC_DIVIDER_MULTIPLIER` from 3.2 to 2.0
- Fix R6/R7 compile-time check in `board_config.h`
- Add gated ADC bootstrap: GPIO1 gate control with BSS138

### 4. Charger Status GPIOs (new)
- Add `components/charger_bq24074/` with API:
  - `charger_init()` — init GPIO5(CE), GPIO6(CHG), GPIO7(FAULT)
  - `charger_get_state()` — returns CHARGER_SRC_USB / CHARGER_CHARGING / CHARGER_DONE / CHARGER_FAULT / CHARGER_BATTERY_ONLY
  - `charger_is_usb_present()` — check CHG/FAULT states

### 5. board_pins.h Updates
```c
#define BOARD_PIN_USB_DP         19
#define BOARD_PIN_USB_DN         18
#define BOARD_PIN_CHARGER_CE      5
#define BOARD_PIN_CHARGER_CHG     6
#define BOARD_PIN_CHARGER_FAULT   7
#define BOARD_PIN_BAT_GATE        1
```

### 6. ADC Settling Time
- Gate HIGH → 500ms settling → ADC read (8-16x avg) → Gate LOW
- Sample every 30-60s
- `adc1_config_channel_atten(ADC1_GPIO0_CHANNEL, ADC_ATTEN_DB_11)` required

### 7. Unchanged Components
- display_sh1106 — NO changes needed
- encoder — NO changes needed
- ui — NO changes needed
- notification_store — NO changes needed
- settings — NO changes needed
- ble_ancs — NO changes needed
- CMakeLists.txt — NO changes needed
