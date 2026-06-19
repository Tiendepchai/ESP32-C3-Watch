# ESP32-C3-MINI-1-N4 Complete 42-Pin Pinout

Source: Espressif ESP32-C3-MINI-1 Datasheet v1.0, Table 2: Pin Definitions

## Mechanical Overview

- 42 castellated pads
- Pins 1-21: left side (when viewed with antenna at top)
- Pins 22-42: right side (when viewed with antenna at top)
- Pin 1 is on the left side, nearest the antenna end
- Pin 22 is on the right side, nearest the antenna end

## Complete Pin Table

| Pin | Name | Type | Description |
|-----|------|------|-------------|
| 1 | VDD_SPI | power_in | Power supply for internal SPI flash (1.8V or 3.3V) |
| 2 | VDD3P3 | power_in | 3.3V power supply |
| 3 | CHIP_EN | input | Chip enable, high=on, low=off |
| 4 | GPIO2 | bidirectional | GPIO2 / ADC1_CH2 / FSPIQ |
| 5 | GPIO3 | bidirectional | GPIO3 / ADC1_CH3 |
| 6 | GPIO4 | bidirectional | GPIO4 / ADC1_CH4 |
| 7 | GPIO5 | bidirectional | GPIO5 / ADC1_CH5 |
| 8 | GPIO6 | bidirectional | GPIO6 / FSPID / MTMS |
| 9 | GPIO7 | bidirectional | GPIO7 / FSPIDO / MTDI |
| 10 | GPIO8 | bidirectional | GPIO8 / FSPICLK / MTCK |
| 11 | GPIO9 | bidirectional | GPIO9 / BOOT |
| 12 | GPIO10 | bidirectional | GPIO10 / FSPICS0 / MTDO |
| 13 | VDD3P3_RTC | power_in | RTC power supply (3.3V) |
| 14 | XTP_32K | input | 32.768 kHz crystal oscillator input |
| 15 | XTN_32K | input | 32.768 kHz crystal oscillator output |
| 16 | GPIO0 | bidirectional | GPIO0 / ADC1_CH0 |
| 17 | GPIO1 | bidirectional | GPIO1 / ADC1_CH1 |
| 18 | GPIO18 | bidirectional | GPIO18 / USB_D- |
| 19 | GPIO19 | bidirectional | GPIO19 / USB_D+ |
| 20 | GPIO20 | bidirectional | GPIO20 / TXD |
| 21 | GPIO21 | bidirectional | GPIO21 / RXD |
| 22 | NC | no_connect | Reserved, do not connect |
| 23 | NC | no_connect | Reserved, do not connect |
| 24 | NC | no_connect | Reserved, do not connect |
| 25 | GND | power_in | Ground |
| 26 | GND | power_in | Ground |
| 27 | GND | power_in | Ground |
| 28 | NC | no_connect | Reserved, do not connect |
| 29 | NC | no_connect | Reserved, do not connect |
| 30 | VDD3P3_CPU | power_in | 3.3V power supply for CPU |
| 31 | VDD3P3 | power_in | 3.3V power supply |
| 32 | VDD3P3 | power_in | 3.3V power supply |
| 33 | GPIO11 | bidirectional | GPIO11 / ADC2_CH0 |
| 34 | GPIO12 | bidirectional | GPIO12 / ADC2_CH1 |
| 35 | GPIO13 | bidirectional | GPIO13 / ADC2_CH2 |
| 36 | GPIO14 | bidirectional | GPIO14 / ADC2_CH3 |
| 37 | GPIO15 | bidirectional | GPIO15 / ADC2_CH4 |
| 38 | GPIO16 | bidirectional | GPIO16 / ADC2_CH5 |
| 39 | GPIO17 | bidirectional | GPIO17 / ADC2_CH6 |
| 40 | NC | no_connect | Reserved, do not connect |
| 41 | GND | power_in | Ground |
| 42 | GND | power_in | Ground |

## Notes

- GPIO0, GPIO2, GPIO5, GPIO8, GPIO9 are strapping pins — their state at reset determines boot mode.
- GPIO9 (BOOT) has internal pull-up. LOW at reset = download mode.
- CHIP_EN must not be left floating. For normal operation, connect to VDD3P3 via pull-up.
- VDD_SPI (pin 1) is the power supply for the internal SPI flash. Connect to 3.3V or leave as per module configuration.
- USB_D- (GPIO18) and USB_D+ (GPIO19) require series termination resistors (27 ohm recommended) when used with Native USB-SERIAL-JTAG.
- NC pins (22, 23, 24, 28, 29, 40) should be left unconnected per Espressif recommendation.

## Pin Types for JSON Output

power_in: VDD3P3, VDD3P3_RTC, VDD3P3_CPU, VDD_SPI, GND
input: CHIP_EN, XTP_32K, XTN_32K
bidirectional: GPIO0-21 (all GPIOs)
no_connect: NC pins
