#!/usr/bin/env python3
"""ESP32 Connection Diagnostic Tool"""

import subprocess
import sys
import time


def run_cmd(cmd):
    result = subprocess.run(cmd, capture_output=True, text=True)
    return result.stdout.strip(), result.stderr.strip(), result.returncode


def check_serial_ports():
    print("=== Checking serial ports ===")
    stdout, stderr, rc = run_cmd(["ls", "/dev/cu.*", "/dev/tty.*"])
    ports = [line for line in stdout.split("\n") if line and "Bluetooth" not in line and "debug-console" not in line]
    if ports:
        print(f"  Found {len(ports)} serial port(s):")
        for port in ports:
            print(f"    {port}")
        return ports
    else:
        print("  No serial ports found (only Bluetooth/debug-console).")
        return []


def check_esptool():
    print("\n=== Checking esptool ===")
    stdout, stderr, rc = run_cmd(["esptool", "version"])
    if rc == 0:
        print(f"  {stdout}")
        return True
    else:
        print("  esptool not installed!")
        return False


def try_connect(port=None):
    print(f"\n=== Trying to connect {'to ' + port if port else '(auto-detect)'} ===")
    cmd = ["esptool", "--no-stub", "chip-id"]
    if port:
        cmd = ["esptool", "--port", port, "--no-stub", "chip-id"]

    stdout, stderr, rc = run_cmd(cmd)
    combined = stdout + "\n" + stderr

    if rc == 0:
        print(f"  SUCCESS! ESP32 responded.")
        print(f"  {combined}")
        return True
    else:
        print(f"  Failed to connect.")
        for line in combined.split("\n"):
            if line.strip():
                print(f"    {line.strip()}")
        return False


def try_bootloader_mode(port):
    print(f"\n=== Trying bootloader mode on {port} ===")
    print("  Hold BOOT button on ESP32, then press RESET (or EN).")
    print("  Release BOOT after 2 seconds.")
    input("  Press Enter when done...")

    cmd = ["esptool", "--port", port, "--baud", "115200", "chip-id"]
    stdout, stderr, rc = run_cmd(cmd)
    combined = stdout + "\n" + stderr

    if rc == 0:
        print(f"  SUCCESS in bootloader mode!")
        print(f"  {combined}")
        return True
    else:
        print(f"  Still failed.")
        for line in combined.split("\n"):
            if line.strip():
                print(f"    {line.strip()}")
        return False


def flash_test_firmware(port):
    print(f"\n=== Flashing minimal test firmware to {port} ===")
    print("  This will write a simple Arduino sketch that blinks the onboard LED")
    print("  and prints 'ESP32 alive!' on Serial every second.")

    sketch = '''
#include <Arduino.h>

void setup() {
    Serial.begin(115200);
    // Try common LED pins on ESP32-S3 dev boards
    pinMode(2, OUTPUT);   // GPIO2 - common on many boards
    pinMode(38, OUTPUT);  // GPIO38 - some S3 boards
    pinMode(48, OUTPUT);  // GPIO48 - some S3 boards
    Serial.println("ESP32 alive! RX-8 dashboard firmware test.");
    Serial.println("Chip: ESP32-S3");
    Serial.print("SDK: ");
    Serial.println(ESP.getSdkVersion());
    Serial.print("Free heap: ");
    Serial.print(ESP.getFreeHeap());
    Serial.println(" bytes");
    Serial.print("Flash size: ");
    Serial.print(ESP.getFlashChipSize() / 1024 / 1024);
    Serial.println(" MB");
}

void loop() {
    digitalWrite(2, !digitalRead(2));
    digitalWrite(38, !digitalRead(38));
    digitalWrite(48, !digitalRead(48));
    Serial.println("ESP32 alive!");
    delay(1000);
}
'''

    sketch_dir = "/var/folders/3t/1qtl67w14dg3v0nc11fty66c0000gn/T/esp32_test"
    import os
    os.makedirs(sketch_dir, exist_ok=True)

    with open(os.path.join(sketch_dir, "esp32_test.ino"), "w") as f:
        f.write(sketch)

    print(f"  Sketch written to {sketch_dir}/")
    print(f"  To flash it, run:")
    print(f"    pio run -e esp32-s3-devkitc-1 -t upload --upload-port {port}")
    print(f"    pio device monitor -p {port} -b 115200")
    print()
    print(f"  Or use esptool directly after compiling with PlatformIO.")


def main():
    print("RX-8 ESP32 Connection Diagnostic")
    print("=" * 40)

    has_esptool = check_esptool()
    if not has_esptool:
        print("\nInstall esptool first: pip3 install esptool")
        sys.exit(1)

    ports = check_serial_ports()

    if not ports:
        print("\n" + "!" * 50)
        print("TROUBLESHOOTING: ESP32 not detected on USB")
        print("!" * 50)
        print("""
  1. CABLE: Make sure you're using a DATA USB cable, not a charge-only cable.
     Many USB-C cables that come with power banks are charge-only.

  2. PORT: Try a different USB port on your Mac (try both USB-C ports).

  3. DRIVER: ESP32-S3 uses a USB-Serial-JTAG or CP210x/CH340 chip.
     - macOS usually has built-in CP210x/CH340 drivers
     - For USB-Serial-JTAG (native S3 USB), no driver needed
     - Check: System Settings -> Privacy & Security -> scroll down ->
       if you see "System Software from developer..." click Allow

  4. POWER: Check if the ESP32's power LED is on. If not, the board
     may not be getting power at all.

  5. BOOT MODE: Hold the BOOT button while plugging in USB.

  6. BOARD: Verify it's actually an ESP32-S3 (not ESP32 or ESP8266).
     Look at the chip markings on the board.

  After fixing, re-run this script.
""")
        print("\nMonitoring for new serial ports (plug in the ESP32 now)...")
        print("Press Ctrl+C to stop.\n")
        try:
            seen = set()
            while True:
                stdout, _, _ = run_cmd(["ls", "/dev/cu.*"])
                current = set(stdout.strip().split("\n"))
                new_ports = current - seen - {"", "/dev/cu.Bluetooth-Incoming-Port", "/dev/cu.debug-console"}
                if new_ports:
                    for port in new_ports:
                        print(f"  NEW PORT DETECTED: {port}")
                        seen.add(port)
                        try_connect(port)
                if not seen:
                    seen = current
                time.sleep(1)
        except KeyboardInterrupt:
            print("\nStopped monitoring.")
        return

    for port in ports:
        if try_connect(port):
            flash_test_firmware(port)
            return

    print("\nDirect connection failed. Trying bootloader mode...")
    for port in ports:
        if try_bootloader_mode(port):
            flash_test_firmware(port)
            return

    print("\nCould not connect to ESP32 on any port.")


if __name__ == "__main__":
    main()
