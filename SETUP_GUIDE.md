# Waveshare ESP32-S3-Touch-LCD-7B — PlatformIO + Arduino Setup Guide

## Hardware

- **Board**: Waveshare ESP32-S3-Touch-LCD-7B (7" 1024x600 RGB LCD with capacitive touch)
- **Module**: ESP32-S3-WROOM-1-N16R8 (16MB QIO flash, 8MB OPI PSRAM)
- **Chip**: ESP32-S3 dual-core Xtensa LX7 @ 240MHz

## PlatformIO Configuration

```ini
[env:lolin_s3_pro]
platform = espressif32
board = lolin_s3_pro
framework = arduino
monitor_speed = 115200
board_build.partitions = default_16MB.csv
```

### Why `lolin_s3_pro` and not `esp32-s3-devkitc-1`?

The `esp32-s3-devkitc-1` board definition does **not** have `BOARD_HAS_PSRAM` or a `memory_type` in its JSON, so PSRAM is completely disabled by default. When you manually add `board_build.arduino.memory_type = qio_opi` + `-DBOARD_HAS_PSRAM` to that board, it **bootloops silently** — the app crashes during OPI PSRAM initialization before UART output is available, so you get zero error messages.

The `lolin_s3_pro` board definition natively includes:
- `memory_type: qio_opi` (QIO flash + OPI PSRAM)
- `-DBOARD_HAS_PSRAM` in `extra_flags`
- `upload.flash_size: 16MB`
- The correct bootloader (`bootloader_qio_80m.elf`)

This combination works out of the box with no extra build flags needed.

### Memory type variants in Arduino-ESP32 v2.0.17 (ESP-IDF 4.4)

| Variant      | Flash  | PSRAM | Works on this board? |
|-------------|--------|-------|---------------------|
| `qio_opi`   | QIO    | OPI   | **YES** (via lolin_s3_pro) |
| `dio_opi`   | DIO    | OPI   | Bootloops silently |
| `opi_opi`   | OPI    | OPI   | Crashes: "Octal Flash EFUSE not configured" |
| `qio_qspi`  | QIO    | QSPI  | Boots but PSRAM=0 (wrong PSRAM type) |
| `dio_qspi`  | DIO    | QSPI  | Boots but PSRAM=0 (wrong PSRAM type) |
| (default)   | QIO    | none  | Boots fine, no PSRAM |

The board has **QIO flash** (not OPI) and **OPI PSRAM** (not QSPI). Only `qio_opi` with a compatible board definition works.

## LCD Configuration

### GPIO Pin Mapping (from Waveshare wiki + verified)

| Signal | GPIO | RGB Data | GPIO |
|--------|------|----------|------|
| HSYNC  | 46   | B3       | 14   |
| VSYNC  | 3    | B4       | 38   |
| DE     | 5    | B5       | 18   |
| PCLK   | 7    | B6       | 17   |
|        |      | B7       | 10   |
|        |      | G2       | 39   |
|        |      | G3       | 0    |
|        |      | G4       | 45   |
|        |      | G5       | 48   |
|        |      | G6       | 47   |
|        |      | G7       | 21   |
|        |      | R3       | 1    |
|        |      | R4       | 2    |
|        |      | R5       | 42   |
|        |      | R6       | 41   |
|        |      | R7       | 40   |

### Timing Parameters (from Waveshare official demo)

```
Pixel clock:    30 MHz
HSYNC pulse:    162
HSYNC back:     152
HSYNC front:    48
VSYNC pulse:    45
VSYNC back:     13
VSYNC front:    3
PCLK active neg: true
```

### ESP-IDF 4.4 API Notes (Arduino-ESP32 v2.x)

The RGB panel struct `esp_lcd_rgb_panel_config_t` in ESP-IDF 4.4 is **different** from ESP-IDF 5.x:
- No `num_fbs` field
- No `bits_per_pixel` field
- No `bounce_buffer_size_px` field
- Use `LCD_CLK_SRC_PLL240M` instead of `LCD_CLK_SRC_DEFAULT`
- `esp_lcd_panel_disp_on_off()` returns `ESP_ERR_NOT_SUPPORTED` for RGB panels — do NOT wrap in `ESP_ERROR_CHECK()`

## IO Expander (CH422G)

The board uses a CH422G I2C IO expander at address **0x24** (SDA=GPIO8, SCL=GPIO9) to control:
- EXIO1: Touch RST
- EXIO2: LCD backlight (DISP)
- EXIO4: SD card CS
- EXIO5: USB/CAN select

To enable backlight + LCD power, set all outputs HIGH:

```cpp
Wire.begin(8, 9);  // SDA=8, SCL=9
// Register 0x02 = direction (0xFF = all outputs)
Wire.beginTransmission(0x24);
Wire.write(0x02); Wire.write(0xFF); Wire.endTransmission();
// Register 0x03 = output value (0xFF = all HIGH)
Wire.beginTransmission(0x24);
Wire.write(0x03); Wire.write(0xFF); Wire.endTransmission();
```

## Serial Output

The board has two USB-C ports:
- **"USB"** (native USB OTG): Used for flashing. ROM bootloader output is visible here. App `Serial` (UART0, GPIO 43/44) is NOT visible here.
- **"USB TO UART"**: Connect to this for app `Serial.println()` debug output.

When connected to the native USB port, you see boot ROM messages and can flash, but `Serial.println()` output goes to the UART pins, not the USB port you're on.

## Key Pitfalls

1. **Board selection matters enormously.** Using `esp32-s3-devkitc-1` with manual PSRAM flags causes silent bootloops. Use `lolin_s3_pro` which has the correct flags baked in.

2. **OPI PSRAM init crashes before UART.** With `dio_opi`/`qio_opi` on `esp32-s3-devkitc-1`, the crash happens so early there's zero serial output — you only see ROM bootloader messages repeating.

3. **Flash is QIO, not OPI.** The `opi_opi` variant will crash with "Octal Flash option selected, but EFUSE not configured" because this board's flash chip doesn't support octal mode.

4. **PSRAM is OPI, not QSPI.** The `dio_qspi`/`qio_qspi` variants boot fine but report PSRAM=0 because QSPI initialization fails on an OPI PSRAM chip.

5. **Resolution: this specific variant is 1024x600.** The Waveshare wiki describes 800x480 for the base model, but the 7B variant with the same form factor is 1024x600. Check the engraving on your specific panel.

6. **`esp_lcd_panel_disp_on_off()` not supported.** On ESP-IDF 4.4 RGB panels, this returns `ESP_ERR_NOT_SUPPORTED`. Using `ESP_ERROR_CHECK()` on it causes an abort/reboot loop.

## Verified Working Configuration

```
PlatformIO espressif32 @ 7.0.1
Arduino-ESP32 v2.0.17 (ESP-IDF 4.4.7)
Board: lolin_s3_pro
Partitions: default_16MB.csv
Flash: 16MB QIO @ 80MHz
PSRAM: 8MB OPI
LCD: 1024x600 RGB565 @ 30MHz PCLK
Free heap after LCD init: ~365KB
Free PSRAM after LCD init: ~4.6MB (after 2 frame buffers + user buffer)
```
