#!/usr/bin/env python3
import obd
import time
import sys

print("=== RX-8 OBD2 Connection Test ===")
print()

ports = obd.scan_serial()
print(f"Scanned serial ports: {ports}")

if not ports:
    print("No ports found via scan, trying /dev/cu.VEEPEAK directly...")
    ports = ["/dev/cu.VEEPEAK"]

for port in ports:
    print(f"\nTrying {port}...")
    try:
        connection = obd.OBD(port, fast=False, timeout=30)
        print(f"  Status: {connection.status()}")
        
        if connection.status() != obd.OBDStatus.CAR_CONNECTED:
            print("  Not connected to car. Trying next port...")
            connection.close()
            continue

        print("\n  Querying supported PIDs...")
        supported = connection.supported_commands
        print(f"  {len(supported)} commands supported")
        
        print("\n--- Live Data ---")
        
        queries = [
            ("RPM", obd.commands.RPM),
            ("Speed", obd.commands.SPEED),
            ("Coolant Temp", obd.commands.COOLANT_TEMP),
            ("Intake Air Temp", obd.commands.INTAKE_TEMP),
            ("Throttle Position", obd.commands.THROTTLE_POS),
            ("Engine Load", obd.commands.ENGINE_LOAD),
            ("Intake Pressure", obd.commands.INTAKE_PRESSURE),
            ("Fuel Level", obd.commands.FUEL_LEVEL),
            ("Battery Voltage", obd.commands.CONTROL_MODULE_VOLTAGE),
        ]
        
        while True:
            print()
            for name, cmd in queries:
                if cmd not in supported:
                    continue
                response = connection.query(cmd, force=True)
                if response.is_null():
                    print(f"  {name:20s}: NO DATA")
                else:
                    print(f"  {name:20s}: {response.value}")
            sys.stdout.flush()
            time.sleep(0.5)
            
    except Exception as exc:
        print(f"  Error: {exc}")

print("\nDone.")
