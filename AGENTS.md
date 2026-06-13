# RX-8 ESP32-S3 OBD2 Dashboard

## Project Overview

Replace the Mazda RX-8 center dash display (7" cavity) with a custom ESP32-S3 + 7" screen that reads live vehicle data from a Bluetooth ELM327 OBD2 adapter.

This is a full custom firmware project. The car is a Mazda RX-8 (Renesis 13B rotary engine) which supports standard OBD2 PIDs.

## Hardware BOM

| Component | Spec | Notes |
|-----------|------|-------|
| MCU | ESP32-S3 dev board | Has enough GPIO and speed for 800x480 display + Bluetooth |
| Display | 7" 800x480 TFT IPS (SPI or parallel RGB) | Fits the OEM display cavity. OLED is an option but 4x more expensive ($40-80 vs $10-20) |
| OBD2 Adapter | ELM327 Bluetooth dongle v1.5 | Avoid v2.1 — v1.5 is more reliable. ~$8 on AliExpress |
| Power | 12V-to-5V buck converter (LM2596 or MP1584EN) | Do NOT use linear regulators (LM7805) — they overheat at 12V→5V |
| Fuse | Inline 2A fuse on 12V side | Safety — place between power tap and buck converter |
| Bezel | 3D printed custom bezel | To fill the gap between the 7" screen and the OEM cavity |

**Total estimated cost: $25-45** (TFT route) or $55-85 (OLED route).

## Power Wiring

```
12V ACC (ignition-switched) → inline 2A fuse → buck converter → USB cable → ESP32-S3
```

### Power source — USE IGNITION-SWITCHED (ACC)

Do NOT tap always-on battery power. The ESP32 will drain the car battery overnight.

Where to tap in the RX-8:
- **Cigarette lighter / accessory socket wires** — already switched, easy to access behind the socket
- **OEM display connector** — if replacing the stock screen, its power wire is already ignition-switched and right where you need it
- **Radio harness ACC wire** (usually red) — switched with ignition

Always verify with a multimeter (probe with key ON vs key OFF) before tapping any wire.

The ESP32-S3 draws ~200-500mA with Bluetooth active, so even the smallest buck converter handles it easily.

## Software Architecture

### Target Stack
- **Framework**: Arduino (ESP32 Arduino core 2.x) or ESP-IDF
- **UI Library**: LVGL (Light and Versatile Graphics Library) — runs directly on ESP32-S3, no OS needed
- **OBD2 Library**: ELMduino (https://github.com/PowerBroker2/ELMduino) — 873 stars, MIT license, handles ELM327 communication over Bluetooth
- **Display Driver**: TFT_eSPI or LVGL's built-in ESP32-S3 display driver
- **Build System**: PlatformIO (recommended) or Arduino IDE

### Key Design Pattern — Abstract OBD Source

The firmware MUST use an abstraction layer for OBD2 data so it can be developed and tested without hardware:

```cpp
class OBDSource {
public:
    virtual float getRPM() = 0;
    virtual float getSpeed() = 0;
    virtual float getCoolantTemp() = 0;
    virtual float getIntakeTemp() = 0;
    virtual float getThrottlePos() = 0;
    virtual float getEngineLoad() = 0;
    virtual float getMAP() = 0;
    virtual float getOilTemp() = 0;
    virtual float getFuelLevel() = 0;
    virtual float getBatteryVoltage() = 0;
};

// MockOBD — for development on PC, returns simulated data (sine waves, noise, ramps)
class MockOBD : public OBDSource {
    float getRPM() override { return 3000 + sin(millis() / 1000.0) * 2000; }
    float getSpeed() override { return 60 + sin(millis() / 2000.0) * 40; }
    // ... etc
};

// ELM327OBD — for production, wraps ELMduino library calls
class ELM327OBD : public OBDSource {
    ELM327 myELM327;
    float getRPM() override { return myELM327.rpm(); }
    // ... etc
};
```

This lets the UI code be completely hardware-independent. Swap MockOBD for ELM327OBD with one line when deploying to hardware.

### Display Architecture

LVGL runs on both PC (SDL2 simulator backend) and ESP32-S3 (display driver backend). The UI code is identical — only the driver init changes:

```
PC:      LVGL + SDL2 backend → renders in a desktop window
ESP32:   LVGL + SPI/RGB driver → renders on physical 7" display
```

Same C/C++ code, same layouts, same gauges, same animations. One-line change for the driver.

## OBD2 PIDs — RX-8 Supported

The RX-8 supports standard OBD2 PIDs. These are the most useful for a dash display:

| PID | Name | Unit | Notes |
|-----|------|------|-------|
| 0x0C | Engine RPM | rpm | Critical — rotary redline at ~9000 RPM |
| 0x0D | Vehicle Speed | km/h | |
| 0x05 | Engine Coolant Temp | °C | Rotary runs hot, monitor this |
| 0x0F | Intake Air Temp | °C | |
| 0x11 | Throttle Position | % | |
| 0x04 | Engine Load | % | Calculated load value |
| 0x0B | Intake Manifold Pressure | kPa | MAP sensor |
| 0x2F | Fuel Tank Level | % | |
| 0x42 | Control Module Voltage | V | Battery/alternator voltage |
| 0x0A | Fuel Pressure | kPa | |
| 0x06-0x09 | Short/Long Term Fuel Trim | % | Banks 1 & 2 |

**Note**: Standard OBD2 does NOT give you rotary-specific data (apex seal health, rotor position, etc.). For that you'd need Mazda-specific CAN bus PID hacking, which is a separate project.

### ELMduino Notes
- Non-blocking API: call `myELM327.rpm()`, check `myELM327.nb_rx_state` until `ELM_SUCCESS`
- Only query ONE PID at a time — wait for response before sending the next
- If connection is unreliable, try 38400 baud instead of 115200
- Use the ELM327's MAC address instead of device name "OBDII" for more reliable Bluetooth pairing on ESP32

### BLE OBD2 Integration

The project includes a fully working BLE OBD2 scanner for the **Veepeak OBDCheck BLE** adapter (ELM327 v1.5) using **NimBLE-Arduino** on ESP32-S3. See `bluetooth/README.md` for:
- BLE service/characteristic UUIDs (`0xFFF0`/`0xFFF1`/`0xFFF2`)
- ELM327 initialization sequence and command format
- PID decoder formulas and reference table
- Speed optimization techniques (`ATST`, fast mode, no-delay loops)
- Troubleshooting guide for common connection issues

Also see `bluetooth/bluetooth_example.cpp` for a minimal standalone sketch that benchmarks PID polling speed.

## Development Workflow

### Phase 1: UI Development (No Hardware Needed)

1. Set up LVGL PC simulator:
   ```bash
   git clone https://github.com/lvgl/lv_port_pc_visualstudio
   # or
   git clone https://github.com/lvgl/lv_port_pc_eclipse
   ```
2. Implement MockOBD with realistic simulated data (sine waves for RPM, gradual temp changes, etc.)
3. Design gauge layouts: RPM (large, prominent), speed, coolant temp, oil temp, engine load, throttle pos, battery voltage
4. Implement touch-friendly navigation between screens
5. Add warning thresholds (overheat, low voltage, high RPM)
6. Iterate on the design visually in the desktop window

### Phase 2: Hardware Integration (When Parts Arrive)

1. Wire up ESP32-S3 + 7" display on the bench
2. Change LVGL display driver from SDL2 to SPI/RGB
3. Swap MockOBD → ELM327OBD
4. Pair with ELM327 Bluetooth dongle, verify OBD2 communication
5. Test in the car — verify all PIDs read correctly
6. Tune refresh rates and screen brightness

### Phase 3: Installation

1. Remove OEM display from RX-8 dash
2. 3D print bezel for the 7" screen
3. Wire power: tap ignition-switched 12V → buck converter → ESP32-S3
4. Mount everything, connect ELM327 dongle to OBD2 port (under driver dash)
5. Test drive and finalize

## Reference Projects

| Project | URL | Stars | Notes |
|---------|-----|-------|-------|
| ESP32-Bluetooth-OBD2-Gauge | https://github.com/VaAndCob/ESP32-Bluetooth-OBD2-Gauge | 445 | Closest existing project. ESP32 + TFT + ELM327 BT. Archived but fully functional. Uses CYD (Cheap Yellow Display). |
| ELMduino | https://github.com/PowerBroker2/ELMduino | 873 | Core OBD2 communication library for Arduino/ESP32. Must-use. |
| Mazda-6-GJ-GL-DPF | https://github.com/maciekelga/Mazda-6-GJ-GL-DPF | 4 | Mazda-specific ESP32 + ELMduino project (DPF gauge). Proves Mazda + ESP32 + OBD2 stack works. |
| ESP32OBDGauge | https://github.com/alonergan/ESP32OBDGauge | 2 | Multi-screen gauge with BLE OBDII. |
| esp32s3-obd-speedometer | https://github.com/ibjelic/esp32s3-obd-speedometer | 1 | ESP32-S3 + round display speedometer. MicroPython. |
| FordMustang_BoostGauge_ESP32-S3 | https://github.com/ClaudeMarais/FordMustang_BoostGauge_ESP32-S3 | 1 | ESP32-S3 boost gauge using OBD2 — similar architecture. |

No RX-8-specific project exists yet. This would be the first.

## RX-8 Specific Notes

- OBD2 port location: under the dash, driver side
- The center display cavity is ~7 inches — verify exact dimensions before ordering screen
- The OEM display is an LCD info screen (trip computer, audio, climate) — it can be removed without affecting other car systems
- The rotary engine runs hot — coolant temp gauge should be prominent with a warning at ~105°C
- Rotary redline is ~9000 RPM (Series 1) or ~8500 RPM (Series 2 PZ) — RPM gauge should go to at least 9000
- Consider adding a "flooded engine" restart counter or warning (RX-8 specific issue — see rx8.lucien.uk docs for flooding info)
- The RX-8 has known ignition coil and spark plug issues — monitoring misfire count through OBD2 would be valuable

## File Structure (Suggested)

```
src/
├── main.cpp                  # Entry point, init display + OBD source
├── obd/
│   ├── obd_source.h          # Abstract OBD interface
│   ├── mock_obd.h            # Mock implementation for PC dev
│   ├── mock_obd.cpp
│   ├── elm327_obd.h          # Real ELM327 implementation
│   └── elm327_obd.cpp
├── ui/
│   ├── dashboard_ui.h        # Main dashboard layout
│   ├── dashboard_ui.cpp
│   ├── gauges.h              # Gauge widgets (RPM, temp, speed, etc.)
│   ├── gauges.cpp
│   ├── screens.h             # Multiple screen navigation
│   ├── screens.cpp
│   └── themes.h              # Color themes and styling
├── config.h                  # Pin definitions, screen resolution, BT settings
└── warnings.h                # Threshold definitions for alerts
```

## Git Rules

- **NEVER run `git commit`, `git push`, or any git mutation without explicit user permission.** Always ask for confirmation first, even if the user has confirmed earlier in the conversation.
- Do not update `.gitignore` or create README/AGENTS files unless explicitly asked.

## Code Style

- Use descriptive variable names — no single or double letter variables (e.g. no `i`, `j`, `x`, `t`, `r`, `g`, `b`). Use names like `strip_index`, `position`, `red`, `green`, `blue`, etc.
- Exception: LVGL API struct fields and function parameters that require short names (e.g. `coords.x1`) are fine as-is.

## ESP32 Display Mode Rule

**NEVER use `LV_DISPLAY_RENDER_MODE_FULL` on the ESP32 target.**

`FULL` mode forces LVGL to re-render the entire 1024×600 screen (1.2MB) on every single frame. On a 240MHz ESP32-S3 this results in ~1–2 FPS and makes the dashboard unusable.

**Preferred approach:** Use `LV_DISPLAY_RENDER_MODE_DIRECT` with double buffering and a bounce buffer. LVGL renders directly into the panel's PSRAM frame buffers with zero-copy. The flush callback must keep both buffers in sync by copying the just-rendered buffer to the other buffer before calling `lv_display_flush_ready()`. This ensures unchanged pixels don't flash black from initial `calloc` state when the display swaps buffers.

`DIRECT` mode with double buffering does **NOT** inherently flicker. Flicker only occurs if the application fails to synchronize the two frame buffers. The reference project (`lv_port_viewe_7_espidf`) uses this exact setup successfully.

`LV_DISPLAY_RENDER_MODE_PARTIAL` is also viable, but `DIRECT` is preferred for RGB panels because it avoids the per-area `esp_lcd_panel_draw_bitmap()` overhead and tearing issues.

## Visual Feedback Workflow

**MANDATORY: After EVERY code change to UI files (`src/ui/*.cpp`, `src/ui/*.h`, `src/main.cpp`), you MUST verify the result before telling the user it's done. Never skip verification.**

1. Build the native target
2. Run screenshot captures at key states: `program.exe --screenshot <rpm> <file.bmp>`
3. **Pixel analysis (primary)**: Read BMP pixels directly via Node.js to verify geometry, alignment, and colors. The BMP is 32-bit RGBA, rowSize=3200, data offset at byte 10. Use this to check exact pixel positions, measure fill heights, detect seams/gaps, and verify color values.
4. **Gemini analysis (secondary)**: Run `node analyze.js <screenshot.png>` (Gemini 3 Flash via Vercel AI SDK) for qualitative visual feedback. Be aware: Gemini's pixel coordinates are unreliable — trust pixel analysis over Gemini's coordinates.
5. Iterate until both pixel analysis and Gemini confirm correctness
6. Only then show the result to the user

### Pixel Analysis Cheat Sheet

```javascript
const fs = require('fs');
const buf = fs.readFileSync('screenshot.bmp');
const offset = buf.readUInt32LE(10);
const rowSize = 3200; // 800px * 4 bytes, already 4-byte aligned
function px(x, y) {
  const row = 479 - y; // BMP is bottom-up, flip Y
  const idx = offset + row * rowSize + x * 4;
  return '#' + [buf[idx+2],buf[idx+1],buf[idx]].map(v=>v.toString(16).padStart(2,'0')).join('');
}
// Example: check fill height at each x position
for (let x = 668; x <= 760; x += 4) {
  let y_first = -1, y_last = -1;
  for (let y = 300; y <= 475; y++) {
    const c = px(x, y);
    if (c !== '#080808' && c !== '#00a931' && c !== '#181818') {
      if (y_first === -1) y_first = y;
      y_last = y;
    }
  }
  if (y_first >= 0) console.log('x='+x+': fill y='+y_first+'..'+y_last);
}
```

Do NOT show the user a broken result. Verify first.