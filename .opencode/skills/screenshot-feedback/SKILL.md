---
name: screenshot-feedback
description: Use after every code change to the LVGL dashboard (rpm_gauge, speed_gauge, any UI file). Builds the native simulator, runs it in screenshot mode, captures screenshots at key RPM/speed values, then analyzes them with ZAI image analysis to verify visual correctness without the user needing to check manually.
---

# Screenshot Feedback Loop

Use this skill EVERY TIME you modify any UI file in `src/ui/` or `src/main.cpp`. The workflow:

## Step 1: Build

```powershell
Stop-Process -Name "program" -Force -ErrorAction SilentlyContinue
$env:PATH = "C:\msys64\mingw64\bin;" + $env:PATH
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e native
```

## Step 2: Capture screenshots

The simulator supports `--screenshot <rpm_value> <output.bmp>` mode. Run it for multiple RPM states:

```powershell
$exe = ".pio\build\native\program.exe"
& $exe --screenshot 0 screenshot_off.bmp
& $exe --screenshot 3000 screenshot_3000.bmp
& $exe --screenshot 7000 screenshot_7000.bmp
& $exe --screenshot 11000 screenshot_11000.bmp
```

## Step 3: Analyze screenshots

Use `zai-mcp-server_analyze_image` for each screenshot:

- **screenshot_off.bmp**: Verify band is fully dark, borders visible, digits show "00000" in dim color
- **screenshot_3000.bmp**: Verify green fill covers correct portion, no gaps between flat and arc sections, border alignment
- **screenshot_7000.bmp**: Verify amber gradient transition, arc corner alignment with flat bands
- **screenshot_11000.bmp**: Verify full red fill, no overflow, all borders intact

For each image, ask: "Describe the RPM gauge band shape, gradient fill, border alignment, and any visual artifacts. Is the band properly aligned with its borders? Is the arc corner smooth? Are there gaps or misalignments?"

## Step 4: Act on feedback

Based on the analysis:
- If alignment is off by N pixels, adjust the relevant constant (ARC_CENTER_X, ARC_CENTER_Y, FILL_INSET, etc.)
- If there are gaps, extend the fill region
- If there are artifacts, investigate the drawing order
- Re-run Steps 1-3 until the analysis confirms correctness

## Implementation

The `--screenshot` flag is handled in `src/main.cpp`. When this flag is present, the program:
1. Initializes LVGL and SDL in headless mode
2. Creates the dashboard
3. Calls `update_simulation()` with the specified RPM value
4. Renders a few frames
5. Captures the SDL window content to a BMP file
6. Exits immediately

The screenshot function uses SDL2's `SDL_RenderReadPixels` and `SDL_SaveBMP`.
