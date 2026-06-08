# ESP32-S3 BLE OBD2 Scanner

Reads live car data from a Veepeak OBDCheck BLE adapter using an ESP32-S3 over Bluetooth LE.

## Hardware

- **MCU**: ESP32-S3 (BLE only, no Bluetooth Classic support)
- **OBD2 Adapter**: [Veepeak OBDCheck BLE](https://www.veepeak.com/product/obdcheck-ble/) (ELM327 v1.5)
- Plug the Veepeak into the car's OBD2 port (under the dashboard)
- Connect the ESP32-S3 to your computer via USB

## Wiring

No wiring between ESP32-S3 and the OBD2 adapter. Communication is entirely wireless over BLE.

## Requirements

- PlatformIO (`pio` command)
- USB cable for ESP32-S3
- Car with ignition ON or engine running
- Veepeak adapter not connected to any phone/app

## Usage

```
pio run --target upload
pio device monitor
```

You should see output like:

```
─────────────────────────────────────
  Engine RPM                 1154 rpm
  Vehicle Speed               0.0 km/h
  Coolant Temp               57.0 C
  Intake Air Temp            42.0 C
  Engine Load               100.0 %
  Throttle Position           6.3 %
  Fuel Level                  (N/A)
  Timing Advance              7.5 deg
  MAF Rate                   10.7 g/s
  Battery Voltage            11.5 V
─────────────────────────────────────
```

Values marked `(N/A)` mean that PID is not supported by the vehicle.

## Architecture

### BLE Communication

The ESP32-S3 does **not** support Bluetooth Classic. The Veepeak OBDCheck BLE advertises on both BLE and Classic, but we use BLE.

**BLE Service/Characteristic Discovery:**

| What | UUID | Role |
|------|------|------|
| Service | `0xFFF0` | OBD2 UART-over-BLE service |
| Write Characteristic | `0xFFF2` | Send commands to ELM327 |
| Notify Characteristic | `0xFFF1` | Receive responses from ELM327 |
| CCCD | `0x2902` | Standard BLE descriptor for notifications |

The adapter advertises as **"VEEPEAK"**.

**Communication flow:**
1. BLE scan for devices named "VEEPEAK"
2. Connect to the device
3. Discover service `0xFFF0`
4. Subscribe to notifications on `0xFFF1`
5. Write ELM327 commands to `0xFFF2`
6. Read responses via notifications on `0xFFF1`

**Important**: Write and notify are on **separate characteristics**. You write commands to `0xFFF2` and receive data on `0xFFF1`. Do NOT try to write and read on the same characteristic.

### ELM327 Protocol

The Veepeak uses an ELM327 v1.5 chipset. Commands are sent as ASCII terminated with `\r` (carriage return, no newline). Responses are terminated with `>`.

**Initialization sequence:**

| Command | Purpose |
|---------|---------|
| `ATZ` | Reset the ELM327 |
| `ATE0` | Disable echo (so responses don't echo commands back) |
| `ATH0` | Disable CAN headers in responses |
| `ATSP0` | Auto-detect OBD2 protocol |

After init, send PID requests like `010C` (RPM) and parse the response.

**Response format (with ATH0):**

```
41 XX DD DD...
```

Where `41` = response to mode 01, `XX` = the PID, and `DD` bytes are the data. Bytes are space-separated hex.

**Example:**
- Request: `010C\r` (engine RPM)
- Response: `41 0C 12 08 >`
- Parse: bytes `12` and `08`, RPM = `(0x12 * 256 + 0x08) / 4 = 1154`

**Special responses to handle:**

| Response | Meaning |
|----------|---------|
| `SEARCHING...` | ELM327 is trying protocols, not ready yet |
| `STOPPED` | Protocol search stopped |
| `NO DATA` | No response from ECU |
| `7F 01 12` | PID not supported by this vehicle |
| `ERROR` | Communication error |

**Battery voltage** is read via the AT command `ATRV`, which returns a decimal like `12.4V`. This is an ELM327 internal reading, not an OBD2 PID.

### OBD2 PID Reference

| PID | Name | Formula | Unit |
|-----|------|---------|------|
| `0104` | Engine Load | `A * 100 / 255` | % |
| `0105` | Coolant Temp | `A - 40` | °C |
| `010C` | Engine RPM | `((A * 256) + B) / 4` | rpm |
| `010D` | Vehicle Speed | `A` | km/h |
| `010E` | Timing Advance | `A / 2 - 64` | degrees |
| `010F` | Intake Air Temp | `A - 40` | °C |
| `0110` | MAF Rate | `(A * 256 + B) / 100` | g/s |
| `0111` | Throttle Position | `A * 100 / 255` | % |
| `012F` | Fuel Tank Level | `A * 100 / 255` | % |

In the formulas, `A` is the first data byte (3rd byte in response after `41 XX`), `B` is the second.

## Porting to Other ESP32 Projects

### Dependencies

In `platformio.ini`:
```ini
lib_deps = h2zero/NimBLE-Arduino@^2.3.1
```

NimBLE is preferred over the default BLE library for lower memory usage and better stability on ESP32-S3.

### Key Implementation Details

1. **Use NimBLE, not the default BLEClient library.** The default ESP32 BLE library has memory issues on ESP32-S3. NimBLE is lighter and more reliable.

2. **Separate write and notify characteristics.** The Veepeak uses `0xFFF2` for writing commands and `0xFFF1` for receiving responses via notifications. You must subscribe to notifications on `0xFFF1` before writing any commands.

3. **Responses come in multiple BLE notifications.** A single OBD2 response may arrive across multiple BLE notification packets. Buffer the data until you receive the `>` terminator character.

4. **Do not strip spaces from responses.** The ELM327 returns space-separated hex bytes (e.g. `41 0C 12 08`). You need spaces to parse individual bytes.

5. **Keep spaces during parsing.** Parse response as `41` `0C` `12` `08` (4 separate hex values). The first two are always the response header (`41` = mode 01 response, `0C` = the PID). Data bytes start at index 2.

6. **Use `\r` not `\r\n`.** ELM327 expects commands terminated with a single carriage return.

7. **Handle protocol negotiation time.** After `ATSP0` (auto protocol), the ELM327 needs time to try different OBD2 protocols. Send `0100` as a test and wait. If you get `SEARCHING...` or `NO DATA`, wait and retry.

8. **ATRV for voltage.** Battery voltage is not a standard OBD2 PID. Use the ELM327 AT command `ATRV` which returns a decimal string like `12.4V`. Parse it as a float.

9. **Not all PIDs are supported.** Each vehicle supports a different subset of PIDs. Use PID `0100` to query which mode 01 PIDs are supported (bitfield response). If a PID returns `7F 01 12`, that PID is not supported.

10. **Auto-reconnect.** BLE connections can drop. Implement reconnection logic that re-discovers the service and re-subscribes to notifications.

## Maximizing Polling Speed

### Benchmark Results (Veepeak OBDCheck BLE, ESP32-S3)

| Metric | Value |
|--------|-------|
| Single PID round-trip (default) | ~225ms = **4.4 Hz** |
| Single PID round-trip (optimized) | faster, depends on `ATST` setting |
| Multi-PID in one request (`010C0D`) | **Not supported** (returns `7F 01 12`) |
| Theoretical ceiling | Set by ELM327 CAN timeout + BLE round-trip |

30 Hz per PID is **not achievable** with this adapter. The bottleneck is the ELM327 querying the ECU over the CAN bus. Each request/response cycle takes ~225ms minimum.

### Optimization Techniques

1. **Reduce ELM327 CAN timeout with `ATST`.** The default timeout is ~200ms. Send `ATST XX` where the value is in units of 4ms. `ATST 10` = 64ms timeout. Lower values risk `NO DATA` responses if the ECU is slow:
   ```
   ATST FF  -> 1020ms (default)
   ATST 20  -> 128ms
   ATST 10  -> 64ms
   ATST 08  -> 32ms
   ```

2. **Don't wait for the `>` prompt.** The ELM327 sends data bytes first, then `>` when ready for the next command. You can return as soon as you have enough hex bytes instead of waiting for `>`. This saves ~10-30ms per request.

3. **Use BLE write without ACK.** Call `writeValue(data, len, false)` instead of `writeValue(data, len, true)`. The `false` parameter skips waiting for the BLE write confirmation, saving a few milliseconds.

4. **Remove all `delay()` calls** between PID requests. Use `yield()` in your wait loop instead of `delay(10)` to keep the watchdog happy without adding latency.

5. **Warm-start the adapter.** Don't send `ATZ` (full reset) every time. If the adapter is already connected to the ECU, just send `ATE0`, `ATH0`, `ATL0` and start polling. `ATZ` forces protocol renegotiation which can fail.

6. **Poll fewer PIDs.** Each PID is a separate CAN request. Polling 3 PIDs at 4 Hz gives an effective 12 data-points-per-second budget. Prioritize what you need.

7. **Multi-PID requests don't work on this adapter.** Sending `010C0D` (RPM + speed in one command) returns `7F 01 12` (not supported). You must query each PID separately.

### Fast Polling Code Pattern

```cpp
// 1. Init: reduce ELM327 timeout
sendAtCmd("ATST10");  // 64ms CAN timeout

// 2. Write without BLE ACK wait
writeChar->writeValue((uint8_t *)"010C\r", 5, false);

// 3. Don't wait for '>' - return when data bytes arrive
unsigned long start = micros();
while (!responseComplete && (micros() - start) < timeout) {
    if (responseHasData && responseBuf has enough bytes) {
        break;  // data is here, don't wait for '>'
    }
    yield();  // no delay()
}

// 4. Immediately send next PID
```

### Speed vs Reliability Tradeoff

| `ATST` Value | Timeout | Speed | Risk |
|---------------|---------|-------|------|
| `FF` (default) | ~1020ms | ~1 Hz | None |
| `40` | 256ms | ~3-4 Hz | Very safe |
| `10` | 64ms | ~4-6 Hz | Some `NO DATA` possible |
| `04` | 16ms | ~8-10 Hz | Frequent missed responses |

Start with `ATST 10` and increase if you get too many `NO DATA` responses. The car's ECU response time varies by make/model/year.

### Minimal Connection Code

```cpp
#include <NimBLEDevice.h>

// After NimBLEDevice::init("MyDevice"):
NimBLEClient *client = NimBLEDevice::createClient();
client->connect("VEEPEAK");  // connects by name

NimBLERemoteService *svc = client->getService("FFF0");
NimBLERemoteCharacteristic *writeChar = svc->getCharacteristic("FFF2");
NimBLERemoteCharacteristic *notifyChar = svc->getCharacteristic("FFF1");

// Subscribe to responses
notifyChar->subscribe(true, [](NimBLERemoteCharacteristic *pChar,
                               uint8_t *pData, size_t length, bool isNotify) {
    // Buffer pData until '>' is received
});

// Send command
writeChar->writeValue("010C\r");
```

## Troubleshooting

| Problem | Cause | Fix |
|---------|-------|-----|
| No serial output | USB CDC vs UART mismatch | Remove `ARDUINO_USB_CDC_ON_BOOT` flag |
| BLE scan finds 0 devices | Wrong NimBLE scan API call | Use `pScan->getResults(duration_ms)` (blocking) |
| "OBD2 characteristic not found" | Wrong UUIDs | Adapter uses `0xFFF0`/`0xFFF1`/`0xFFF2`, not `0xFFE0`/`0xFFE1` |
| "SEARCHING..." forever | Ignition not ON | Turn key to ON or start engine |
| All PIDs show (N/A) | Response parsing broken | Keep spaces in response, parse byte by byte |
| Speed shows 13 km/h at 0 | Wrong byte index in decoder | Data bytes start at index 2 (after `41 XX` header) |
| Values like 127 kPa or 1700 L/h | Same byte index bug | PID byte was being read as data byte |
| "NO DATA" after working | Connection dropped or ECU timeout | Wait a few seconds, it usually recovers |
| Port busy during upload | Serial monitor still open | Kill the monitor process before uploading |
