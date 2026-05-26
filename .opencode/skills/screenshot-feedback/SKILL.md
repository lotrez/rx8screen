---
name: screenshot-feedback
description: Use after every code change to UI files (rpm_gauge, speed_gauge, any UI file). Builds the native simulator, captures screenshots, then verifies correctness using pixel-level BMP analysis (primary) and Gemini 3 Flash (secondary).
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

```powershell
$exe = ".pio\build\native\program.exe"
& $exe --screenshot 3000 screenshot_3000.bmp
& $exe --screenshot 7000 screenshot_7000.bmp
& $exe --screenshot 11000 screenshot_11000.bmp
```

## Step 3: Pixel analysis (PRIMARY — always do this first)

Read BMP pixels directly via inline Node.js. The BMP is 32-bit RGBA, rowSize=3200, data offset at byte 10, 800x480. Flip Y (BMP is bottom-up).

```javascript
// Run with: node -e "..." or save as check.js
const fs = require('fs');
const buf = fs.readFileSync('screenshot_7000.bmp');
const offset = buf.readUInt32LE(10);
const rowSize = 3200;
function px(x, y) {
  const row = 479 - y;
  const idx = offset + row * rowSize + x * 4;
  return '#' + [buf[idx+2],buf[idx+1],buf[idx]].map(v=>v.toString(16).padStart(2,'0')).join('');
}
```

Key checks to perform:

### Border continuity
Scan along border positions for `#00a931` (or `#00aa30`):
- Inner bottom: y=420, x from MARGIN to ARC_CENTER_X
- Outer bottom: y=470, x from MARGIN to ARC_CENTER_X
- Inner right: x=740, y from ARC_CENTER_Y to MARGIN
- Outer right: x=790, y from ARC_CENTER_Y to MARGIN
- Inner arc: radius 75 from ARC_CENTER
- Outer arc: radius 125 from ARC_CENTER

### Black gap between border and fill
Check y=423 and y=467 (between border at 420/470 and fill at 425/465) for `#080808` at various x positions.

### Fill coverage and gradient
At each x position, find the y range of non-black, non-border pixels. Should be consistent (~40px with FILL_INSET).

### Arc fill consistency
Scan x=668..760 in steps of 4, measuring fill y-range at each. Should be smooth and continuous (no drops or spikes).

### Transition seams
Check x=ARC_CENTER_X±3 at y=fill_mid for any black pixels (gap between flat strips and arc).

### Start/end cap gaps
Check x=MARGIN±3 for black gap between border and fill.

## Step 4: Gemini analysis (SECONDARY — qualitative sanity check)

Convert BMP to PNG first:
```powershell
Add-Type -AssemblyName System.Drawing
foreach ($name in @("screenshot_3000", "screenshot_7000", "screenshot_11000")) {
    $bmp = [System.Drawing.Image]::FromFile("$PWD\$name.bmp")
    $bmp.Save("$PWD\$name.png", [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}
```

Then: `node analyze.js screenshot_7000.png`

**WARNING**: Gemini's pixel coordinates are unreliable. Trust pixel analysis over Gemini's coordinates. Use Gemini only for qualitative observations ("there's a seam", "the gradient looks banded") not quantitative ones ("the seam is at x=830").

## Step 5: Act on feedback

Based on the analysis:
- If pixel analysis shows a gap at specific coordinates, extend/adjust the relevant fill region
- If alignment is off by N pixels, adjust the relevant constant
- If there are artifacts, investigate the drawing order
- Re-run Steps 1-3 until pixel analysis confirms correctness
- Then optionally run Step 4 (Gemini) for final sanity check

## Prerequisites

- `analyze.js` in project root — uses Vercel AI SDK (`ai`, `@ai-sdk/google`)
- `.env` file with `GEMINI_API_KEY=<key>` (gitignored)
- Node.js with `npm install ai @ai-sdk/google`
