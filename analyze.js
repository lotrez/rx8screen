const fs = require('fs');
const path = require('path');

const screenshotPath = process.argv[2];
const provider = process.argv[3] || 'lmstudio';

if (!screenshotPath) {
  console.error('Usage: node analyze.js <screenshot.png> [google|ollama|lmstudio]');
  process.exit(1);
}

const imagePath = path.resolve(screenshotPath);
const imageBuffer = fs.readFileSync(imagePath);
const imageBase64 = imageBuffer.toString('base64');
const ext = path.extname(imagePath).slice(1);

const prompt = `You are an automotive UI designer who makes decisions, not just gives feedback. You are designing a 1024x600 dashboard for a Mazda RX-8, built with LVGL on ESP32 (no images/icons/complex gradients — only solid colored rectangles, bars, arcs, and bitmap fonts).

Current layout:
- Row 0 (45%): Speed hero card — 3 large DSEG7 digits (160px), gear indicator bottom-right
- Row 1 (20%): Gauge card — 3 horizontal gauges (water temp, battery, fuel) with bars
- Row 2 (35%): RPM card — DSEG7 digits (64px) top-left, 30 segmented bars with zone colors

Current palette:
- Background: #0A0A0E, Cards: #111116
- Primary text: #E0E4F0, Dim labels: #5A5E6A
- RPM normal zone: #E0E4F0 (white), warning: #FFB700 (amber), danger: #E53935 (red)
- Gauge bars use same palette (green normal, amber warn, red danger via threshold)

Make these specific design decisions:

1. RPM BAND: Should the segments be (a) all-white for normal with amber/red only for warning zones, or (b) use a green→amber→red gradient across all segments? Pick one. Justify in 1 sentence.

2. GAUGE ROW: The 3 bar gauges (water temp, battery, fuel) are in a single card. Should they (a) keep vertical bar orientation, (b) switch to horizontal bars, or (c) use arc/radial mini-gauges? Pick one. Justify in 1 sentence.

3. COLOR: Should the gauge bars use the same zone colors as RPM (white→amber→red based on thresholds), or should they each have their own color identity? Pick one. Justify in 1 sentence.

4. LAYOUT: Is the current row split (45/20/35) optimal, or should it be adjusted? If yes, give exact new percentages. Justify in 1 sentence.

5. MISSING: What single additional element would most improve this dashboard? Pick from: oil pressure gauge, clock, trip odometer, shift indicator, engine load %. Justify in 1 sentence.

6. RATE: Rate current state 1-10.`;

async function runGoogle() {
  const { generateText } = require('ai');
  const { google } = require('@ai-sdk/google');
  const result = await generateText({
    model: google('gemini-2.0-flash'),
    messages: [
      {
        role: 'user',
        content: [
          { type: 'image', image: `data:image/${ext};base64,${imageBase64}` },
          { type: 'text', text: prompt },
        ],
      },
    ],
  });
  console.log(result.text);
}

async function runOllama() {
  const { generateText } = require('ai');
  const { ollama } = require('ollama-ai-provider');
  const result = await generateText({
    model: ollama('gemma3:12b'),
    messages: [
      {
        role: 'user',
        content: [
          { type: 'image', image: `data:image/${ext};base64,${imageBase64}` },
          { type: 'text', text: prompt },
        ],
      },
    ],
  });
  console.log(result.text);
}

async function runLMStudio() {
  const response = await fetch('http://localhost:1234/api/v1/chat', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      'Authorization': 'Bearer sk-lm-H83pxaKF:knbIWXiHRDo9Re3Deyq5',
    },
    body: JSON.stringify({
      model: 'google/gemma-4-e4b',
      input: [
        { type: 'image', data_url: `data:image/${ext};base64,${imageBase64}` },
        { type: 'text', content: prompt },
      ],
    }),
  });
  const data = await response.json();
  if (data.error) throw new Error(JSON.stringify(data.error));
  const msg = data.output.find(o => o.type === 'message');
  console.log(msg ? msg.content : JSON.stringify(data));
}

const providers = { google: runGoogle, ollama: runOllama, lmstudio: runLMStudio };
(providers[provider] || runLMStudio)().catch(err => { console.error('Error:', err.message); process.exit(1); });
