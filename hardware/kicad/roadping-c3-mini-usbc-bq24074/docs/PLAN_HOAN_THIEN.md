# Plan hoàn thiện: RoadPing C3 Mini USB-C BQ24074

**Ngày:** 2026-06-17
**Target:** 2 tuần (17/06 → 01/07)
**Board size:** 40×40mm, 4-layer, ENIG
**Assembly:** JLCPCB assembled (SMT)
**EVT v2 dependency:** Không — chạy độc lập

---

## TỔNG QUAN

Project đang ở phase **Schematic DRAFT** — schematic đã có đủ linh kiện, kết nối cơ bản, nhưng:
- PCB skeleton có footprints đặt sai vị trí (tọa độ schematic, không phải layout thực)
- **BQ24074 đang dùng sai footprint** (VQFN-16 3×3mm thay vì WQFN-20 3.5×3.5mm)
- Nhiều component chưa nối dây đầy đủ (ERC còn violation)
- Cần layout lại hoàn toàn cho form factor 40×40mm

---

## PHASE 0: CRITICAL FIX — Ngay lập tức (0.5 ngày)

### 0.1 Sửa footprint BQ24074
**Vấn đề:** PCB đang dùng `VQFN-16-1EP_3x3mm_P0.5mm_EP1.6x1.6mm` — SAI.
**Cần:** `Package_DFN_QFN:WQFN-20-1EP_3.5x3.5mm_P0.5mm_EP2.7x2.7mm` — ĐÚNG.

Pin mapping WQFN-20:
| Pin | Tên | Net | Pin | Tên | Net |
|-----|-----|-----|-----|-----|-----|
| 1 | IN | VBUS | 11 | /FAULT | FLT_STAT |
| 2 | IN | VBUS | 12 | OUT | SYS_RAW |
| 3 | VSS | GND | 13 | BAT | VBAT |
| 4 | ISET | 1.1k→GND | 14 | BAT | VBAT |
| 5 | ILIM | 2.0k→GND | 15 | NC | — |
| 6 | TS | 10k→OUT+10k→GND | 16 | NC | — |
| 7 | /CE | CE_CHG | EP | GND | GND + thermal vias |
| 8 | OUT | SYS_RAW | 17 | PG | NC |
| 9 | PG | NC | 18 | TMR | GND (float = 5h) |
| 10 | /CHG | CHG_STAT | 19 | ITERM | GND (float = C/10) |
| | | | 20 | ISET2 | GND (float = not used) |

### 0.2 Dùng schematic generator script
`scripts/gen_sch.py` đã generate schematic đúng. Không chỉnh tay .kicad_sch.
Muốn sửa schematic → sửa `gen_sch.py` rồi chạy lại.

### 0.3 Sửa PCB skeleton
PCB hiện tại:
- Có ~4200+ lines footprint definitions đặt sai vị trí (tọa độ schematic như 55,30 cho module)
- Nhiều footprints chưa có net connection đúng
- Board outline và Edge.Cuts chưa có (trống)

**Cần:** Xóa hết footprint cũ, thay bằng PCB skeleton sạch, layout lại từ đầu.

---

## PHASE 1: 40×40mm PLACEMENT — 2 ngày

### 1.1 Board outline + Stackup
- **Kích thước:** 40.0 × 40.0mm
- **Edge.Cuts:** Rounded corners (3mm radius)
- **Mounting holes:** 4× M2 NPTH (2.2mm drill, 5.0mm copper keepout) tại 4 góc, nếu còn chỗ
- **Stackup:** F.Cu / In1.Cu(GND) / In2.Cu(PWR) / B.Cu, 1.6mm

### 1.2 Component placement — ràng buộc

Module ESP32-C3-MINI-1-N4 (15.5×20.0mm) chiếm phần lớn diện tích.
Đặt module ở center, các component khác xếp xung quanh.

**Sơ đồ placement đề xuất (40×40mm, nhìn từ trên xuống):**

```
┌──────────────────────────────────┐
│  USB-C (J1)         [cạnh dưới] │
├──────────────────────────────────┤
│  USBLC6          BQ24074    C    │
│  (D2)            (U1)      cap  │
│  PESD5V0S1BA     AP2112K        │
│  (D1)            (U2)           │
├──────────────────────────────────┤
│      ESP32-C3-MINI-1-N4 (U3)    │
│      15.5×20.0mm                │
│         ┌───────┐               │
│         │       │               │
│         │       │               │
│         └───────┘               │
├──────────────────────────────────┤
│  JST-PH  OLED     BOOT  Debug   │
│  (J2)    header   (SW2) testpad │
│  [trái]  [phải]                 │
└──────────────────────────────────┘
```

**Vị trí cụ thể đề xuất (gốc tọa độ dưới-trái):**
- U3 (ESP32-C3-MINI-1): trung tâm board — x=20, y=20
  - Module hướng antenna lên trên (y=25..40)
- J1 (USB-C): cạnh dưới — x=12, y=0.5
- U1 (BQ24074): x=10, y=8
- U2 (AP2112K): x=22, y=6
- D2 (USBLC6-2SC6): x=8, y=4 (gần USB-C)
- F1 (PTC VBUS): x=16, y=5
- D1 (TVS): x=14, y=4
- J2 (JST-PH): cạnh trái — x=0, y=20 (xoay 90°)
- DS1 (OLED header): cạnh phải — x=37, y=20
- SW2 (BOOT button): x=30, y=10
- Test pads (UART, SCL, GND, 3V3): cạnh trên — x=5..20, y=38
- R_CC1, R_CC2 (5.1k): gần J1
- F2 (PTC VBAT): gần J2
- R_ISET, R_ILIM, R_TS: gần U1
- BSS138 + divider (R10/R11/R12 + C9): gần U3 góc y=25..30
- R7/R8/R9 (encoder pull-ups): gần header encoder

### 1.3 Encoder PEC11R — Quyết định đặc biệt

PEC11R-4215F-S0024 body 12.5×13.4mm → **KHÔNG fit trong 40×40mm** cùng với ESP32-C3 module.

**Lựa chọn:**
1. **Off-board encoder** — Header 5-pin (A, B, SW, C, GND) + flex cable → encoder gắn trên khung/enclosure riêng
   - Recommended cho 40×40mm
   - Giữ nguyên firmware (GPIO2/3/4)
   - Cần chọn connector nhỏ (Molex PicoBlade 1.25mm hoặc JST SH 1.0mm)
2. **THT encoder on edge** — Đặt PEC11R chồm ra ngoài board edge, phần body nằm ngoài PCB
   - Phần thân encoder nằm ngoài board, chỉ pins + support pads trên PCB
   - Cần kiểm tra cơ khí với enclosure
3. **Bỏ encoder** → chỉ dùng nút, mất navigation UX

**Đã chọn: Option 1 (off-board, header 5-pin)** — đơn giản, linh hoạt.

Chi tiết header encoder:
- Connector: JST SH 1.0mm 5-pin (hoặc Molex PicoBlade 1.25mm)
- Pins: 1 = GND, 2 = C (common), 3 = A, 4 = B, 5 = SW
- Pull-ups (10k) vẫn ở main board
- Debounce cap (10nF) optional trên main board

### 1.4 Kiểm tra JLCPCB assembly constraints

| Item | Yêu cầu |
|------|---------|
| PCB panelization | 40×40mm OK (≥10×10mm) |
| Min component | 0402 OK, dùng 0603 cho dễ |
| ESP32-C3-MINI-1-N4 | Kiểm tra stock JLCPCB (là basic part?) |
| BQ24074RGTR | Kiểm tra stock (WQFN-20) |
| TYPE-C-31-M-12 | Kiểm tra stock |
| USBLC6-2SC6 | Kiểm tra stock |
| BSS138 | Standard part |
| AP2112K-3.3TRG1 | Kiểm tra stock |
| Extended parts | Nếu cần, chấp nhận phí extended component |

---

## PHASE 2: ROUTING — 3 ngày

### 2.1 Net classes (KiCad)
| Net Class | Width | Gap | Via | Layer |
|-----------|-------|-----|-----|-------|
| POWER_HIGH (VBUS, SYS_RAW) | 0.5mm | 0.2mm | 0.6/0.3mm | F.Cu → via → In2.Cu/PWR |
| POWER_BAT (VBAT, BAT_CONN+) | 0.5mm | 0.2mm | 0.6/0.3mm | F.Cu → In2.Cu/PWR |
| POWER_3V3 (3V3) | 0.4mm | 0.2mm | 0.6/0.3mm | F.Cu → star topology |
| GND | 0.4mm | 0.2mm | 0.6/0.3mm | In1.Cu plane |
| USB_DIFF (D+/D-) | 0.3mm | 0.15mm | — | **F.Cu only** — no vias |
| SIGNAL (I2C, UART, ENC) | 0.25mm | 0.2mm | 0.6/0.3mm | F.Cu / B.Cu |
| ANALOG (BAT_ADC) | 0.25mm | 0.25mm | 0.6/0.3mm | F.Cu, guard ring |

### 2.2 Manual routing sequence (quan trọng — theo đúng thứ tự)

1. **USB diff pair** (D+/D-) — ưu tiên #1
   - F.Cu, 90Ω differential, không via
   - USBLC6 → ESP32-C3 D+/D- pins
   - Length match <2mm
2. **Power routing manual**
   - VBUS: J1 → F1 → U1(IN)
   - SYS_RAW: U1(OUT) → U2(VIN)
   - 3V3: U2(VOUT) → U3, DS1, pull-ups
   - VBAT: U1(BAT) → F2 → J2
3. **BQ24074 support**
   - CE, CHG, FAULT → ESP32-C3 GPIO5/6/7
   - ISET (R3=1.1k), ILIM (R4=2.0k)
   - TS divider (R5, R6 = 10k)
   - Decoupling caps sát pins U1
4. **AP2112K capacitor loops** (C1, C2 sát U2)
5. **I2C** (SCL=GPIO10, SDA=GPIO8)
6. **UART debug** (TX=GPIO21, RX=GPIO20)
7. **Encoder/BOOT header** signals
8. **Gated ADC** (BSS138 + divider + filter)
9. **GND stitching vias** — ≥8 vias quanh module U3

### 2.3 In1.Cu GND plane
- Liên tục, không split
- Antenna keepout ≥15mm từ module antenna edge (tra datasheet MINI-1-N4)
- Thermal vias cho BQ24074 EP (≥4×0.3mm)

### 2.4 In2.Cu PWR plane
- Power distribution (SYS_RAW, VBAT)
- Không signal routing trên In2

### 2.5 DRC target
- 0 errors
- 0 unconnected
- 0 clearance violations
- Chấp nhận warning-only: `lib_footprint_mismatch` (nếu có)

---

## PHASE 3: MANUFACTURING EXPORT — 1 ngày

### 3.1 JLCPCB fabrication outputs
- Gerber X2 (JLCPCB format)
  - F.Cu, In1.Cu_GND, In2.Cu_PWR, B.Cu
  - F.Mask, B.Mask
  - F.SilkS, B.SilkS
  - F.Paste, B.Paste
  - Edge.Cuts
  - Drill: PTH + NPTH
- Output: `hardware/fabrication/roadping-c3-mini-usbc-bq24074/`

### 3.2 JLCPCB assembly BOM
Format JLCPCB CPL + BOM CSV:
- **BOM CSV:** Reference, MPN, Manufacturer, Qty, Value, Package, JLCPCB Part #
- **CPL/PNP CSV:** Reference, X, Y, Rotation, Layer (top/bottom)

### 3.3 JLCPCB special requirements
- **PCB Qty:** 5 assembled + 5 bare
- **PCBA side:** Top-side assembly only (bottom-side = test pads)
- **Stencil:** Include (JLCPCB default)
- **PCB color:** Green (default)
- **Surface finish:** ENIG (JLCPCB standard)
- **Copper weight:** 1oz all layers

### 3.4 Design for JLCPCB checklist
- [ ] Tất cả component có sẵn ở JLCPCB (basic/standard/extended)
- [ ] Không dùng part out-of-stock
- [ ] BOM CSV format đúng JLCPCB template
- [ ] Placement file (CPL) format đúng
- [ ] Panelization? 40×40mm, edge rails nếu cần
- [ ] LCSC part numbers cho mỗi component

### 3.5 Gerber review
- Xem từng layer preview (F.Cu, In1, In2, B.Cu, F.Mask, F.SilkS)
- Kiểm tra antenna keepout
- Kiểm tra silkscreen labels đọc được
- Kiểm tra drill map

---

## PHASE 4: FIRMWARE MIGRATION — 2 ngày (chạy song song)

### 4.1 Sửa `board_config.h`
```c
#define BOARD_NAME "RoadPing-C3-Mini"
#define BOARD_REVISION "EVT-1"
#define FIRMWARE_VERSION "0.2.0"

// ADC divider: 470k+470k gated (ratio=2.0)
#define BAT_ADC_R_TOP_OHMS 470000U
#define BAT_ADC_R_BOTTOM_OHMS 470000U
// compile-time check: ratio = 2.0
#define BAT_ADC_DIVIDER_MULTIPLIER 2.0f

// Thresholds with gated ADC (VBAT×0.5)
#define BOARD_BATTERY_LOW_MV 3500U
#define BOARD_BATTERY_CRITICAL_MV 3300U
```

### 4.2 Sửa `board_pins.h`
```c
#define BOARD_PIN_USB_DP         19
#define BOARD_PIN_USB_DN         18
#define BOARD_PIN_CHARGER_CE      5
#define BOARD_PIN_CHARGER_CHG     6
#define BOARD_PIN_CHARGER_FAULT   7
#define BOARD_PIN_BAT_GATE        1
// Các pin còn lại giữ nguyên (UART, I2C, encoder, ADC)
```

### 4.3 USB-SERIAL-JTAG console
- sdkconfig: `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y`
- UART GPIO20/21 giữ làm test pads fallback
- Partition table: 4MB, 2 OTA (`CONFIG_PARTITION_TABLE_TWO_OTA=y`)

### 4.4 Tạo `components/charger_bq24074/`
Component mới với API:
```c
typedef enum {
    CHARGER_SRC_USB,        // USB present, battery charging or done
    CHARGER_CHARGING,       // /CHG=low, /FAULT=high
    CHARGER_DONE,           // /CHG=high, /FAULT=high
    CHARGER_FAULT,          // /FAULT=low
    CHARGER_BATTERY_ONLY,   // No USB
} charger_state_t;

esp_err_t charger_init(void);
charger_state_t charger_get_state(void);
bool charger_is_usb_present(void);
```

### 4.5 Sửa battery ADC
- GPIO1 HIGH → delay 500ms → ADC read → GPIO1 LOW
- `adc1_config_channel_atten(ADC1_GPIO0_CHANNEL, ADC_ATTEN_DB_11)`
- Sample interval: 30-60s

### 4.6 Component KHÔNG cần sửa
- `display_sh1106` — giữ nguyên
- `encoder` — giữ nguyên (cùng GPIO2/3/4)
- `ui` — giữ nguyên
- `notification_store` — giữ nguyên
- `settings` — giữ nguyên
- `ble_ancs` — giữ nguyên
- `CMakeLists.txt` — giữ nguyên

---

## PHASE 5: BOM HOÀN CHỈNH — 1 ngày

### 5.1 BOM tracking
| Ref | MPN | Qty | Package | JLCPCB Part # | Cost |
|-----|-----|:---:|---------|:-------------:|:----:|
| U3 | ESP32-C3-MINI-1-N4 | 1 | 42-pin cast. module | ? | ~$2.50 |
| U1 | BQ24074RGTR | 1 | WQFN-20 | ? | ~$1.20 |
| U2 | AP2112K-3.3TRG1 | 1 | SOT-23-5 | C12345 | ~$0.15 |
| D2 | USBLC6-2SC6 | 1 | SOT-23-6 | ? | ~$0.30 |
| D1 | PESD5V0S1BA | 1 | SOD-323 | ? | ~$0.10 |
| Q1 | BSS138 | 1 | SOT-23 | C12345 | ~$0.05 |
| J1 | TYPE-C-31-M-12 | 1 | 16-pin SMT | ? | ~$0.35 |
| J2 | B2B-PH-K-S | 1 | 2-pin THT | ? | ~$0.15 |
| F1,F2 | 1206L110SLWR | 2 | 1206 | ? | ~$0.15 |
| R_CC1,R_CC2 | 5.1kΩ 0603 | 2 | 0603 | C12345 | ~$0.02 |
| R_ISET | 1.1kΩ 0603 | 1 | 0603 | C12345 | ~$0.01 |
| R_ILIM | 2.0kΩ 0603 | 1 | 0603 | C12345 | ~$0.01 |
| R_TS1,R_TS2 | 10kΩ 0603 | 2 | 0603 | C12345 | ~$0.02 |
| R_CE,R_CHG,R_FLT | 10kΩ 0603 | 3 | 0603 | C12345 | ~$0.03 |
| R_ENC1,2,3 | 10kΩ 0603 | 3 | 0603 | C12345 | ~$0.03 |
| R_ADC_TOP | 470kΩ 0603 | 1 | 0603 | C12345 | ~$0.01 |
| R_ADC_BOT | 470kΩ 0603 | 1 | 0603 | C12345 | ~$0.01 |
| R_GATE_PD | 100kΩ 0603 | 1 | 0603 | C12345 | ~$0.01 |
| C_VBUS_IN | 10µF 0603 | 1 | 0603 | C12345 | ~$0.05 |
| C_VBUS_BYP | 0.1µF 0603 | 1 | 0603 | C12345 | ~$0.01 |
| C_SYS_IN | 10µF 0603 | 1 | 0603 | C12345 | ~$0.05 |
| C_SYS_BYP | 0.1µF 0603 | 1 | 0603 | C12345 | ~$0.01 |
| C_BAT | 1µF 0603 | 1 | 0603 | C12345 | ~$0.02 |
| C_IN_LDO | 1µF 0603 | 1 | 0603 | C12345 | ~$0.01 |
| C_OUT_LDO | 1µF 0603 | 1 | 0603 | C12345 | ~$0.01 |
| C_BULK_3V3 | 10µF 0603 | 1 | 0603 | C12345 | ~$0.05 |
| C_DEC_3V3 | 0.1µF 0603 | 4 | 0603 | C12345 | ~$0.04 |
| C_ADC_FILT | 0.1µF 0603 | 1 | 0603 | C12345 | ~$0.01 |
| DS1 | SH1106 OLED 128x64 | 1 | 4-pin module | — | ~$5.00 |
| SW1 | PEC11R-4215F-S0024 | 1 | Radial THT | — | ~$1.20 |
| | | | **Estimated total BOM** | | **~$11.50** |

### 5.2 Kiểm tra JLCPCB stock
Cần tra cứu LCSC part number cho từng component trước khi đặt.
Những part critical:
- **ESP32-C3-MINI-1-N4** — nếu JLCPCB không stock, có thể mua riêng và gửi kèm (customer parts)
- **BQ24074RGTR** — WQFN-20, thường có stock
- **TYPE-C-31-M-12** — LCSC part C398303

---

## PHASE 6: SẴN SÀNG ĐẶT HÀNG + TEST — 1 ngày

### 6.1 Pre-order checklist
- [ ] ERC: 0 violations
- [ ] DRC: 0 errors, 0 unconnected, 0 clearance
- [ ] Schematic parity: 0 issues
- [ ] BOM CSV xuất đúng format JLCPCB
- [ ] PNP/CPL CSV xuất đúng format
- [ ] Gerber preview OK (từng layer)
- [ ] Antenna keepout clear trên 4 layers
- [ ] Silkscreen labels đọc được
- [ ] JLCPCB impedance coupon cho 90Ω diff pair
- [ ] PCB dimension = 40×40mm

### 6.2 Order JLCPCB
- 5× assembled (top-side)
- 5× bare PCBs
- Lead time: 5-8 ngày (PCB) + 3-5 ngày (assembly)

### 6.3 Chờ board + bring-up
Khi có board, theo trình tự bring-up trong `docs/11_BRINGUP_AND_TEST.md`.

---

## SCHEDULE TỔNG THỂ

| Phase | Task | Days | Bắt đầu | Kết thúc |
|:-----:|------|:----:|:-------:|:--------:|
| 0 | Critical fixes | 0.5 | D0 | D0 |
| 1 | Placement 40×40mm | 2 | D0.5 | D2.5 |
| 2 | Routing | 3 | D2.5 | D5.5 |
| 3 | Manufacturing export | 1 | D5.5 | D6.5 |
| 4 | Firmware migration | 2 | D0 | D2 (song song) |
| 5 | BOM hoàn chỉnh | 1 | D5.5 | D6.5 |
| 6 | Order + test prep | 1 | D6.5 | D7.5 |

→ **ORDER ngày D7-D8** (khoảng 25-26/06)
→ **Board về D12-D15** (khoảng 01-04/07)

---

## VẤN ĐỀ CẦN QUYẾT ĐỊNH

| # | Issue | Lựa chọn |
|:-:|-------|----------|
| 1 | **PEC11R encoder** | Off-board (header 5-pin + flex) — khuyến nghị |
| 2 | **JLCPCB ESP32-C3-MINI-1 stock** | Nếu out-of-stock → gửi kèm (customer part) |
| 3 | **BQ24074 PG pin** (pin 17) | NC hay nối LED? Hiện tại NC |
| 4 | **USB series resistor** | 27Ω trên D+/D-? Cần hay không? (JLCPCB có 0Ω option) |
| 5 | **NTC on battery** | Phase 2, bỏ qua cho EVT này |
