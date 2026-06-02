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

const prompt = `You are an automotive UI designer given full creative authority over this 1024x600 Mazda RX-8 dashboard (ESP32, LVGL primitives only: rectangles, arcs, bars, DSEG7/Orbitron fonts, solid colors). Do NOT suggest minor tweaks like moving elements 10px. Think big — propose real UX features.

Current layout:
- Row 0 (50%): Speed hero — 3 DSEG7 digits, gear corner, km/h label
- Row 1 (25%): Gauge card — WATER TEMP, BATTERY, FUEL horizontal bars (green/amber/red)
- Row 2 (25%): RPM card — DSEG7 digits right-aligned, solid fill bar (white/amber/red zones)

Palette: bg #0A0A0E, cards #111116, text #E0E4F0, dim #5A5E6A, amber #FFB700, red #E53935

Your GOAL: create a dashboard that feels premium, immersive, and purpose-built for a rotary engine enthusiast. It should feel like a product, not a prototype.

Make THREE decisions. For each, explain what you would build and why:

1. SIGNATURE FEATURE: Propose one unique/interactive element that ONLY an RX-8 dashboard would have. No other car — what makes this dashboard special? Could be a visual effect, a rotary-specific gauge, a unique data display.

2. LAYOUT APPROACH: Is the current card-stack layout the right paradigm, or would a different approach work better? Options: (a) keep cards, (b) flow layout with overlapping elements, (c) single unified display with zones, (d) something else. Pick one and sketch the concept.

3. COLOR & MOTION: Beyond static colors, what visual feedback would make this feel alive? Options: (a) RPM-reactive background glow, (b) speed-dependent element scaling, (c) warning animations, (d) something else. Pick one and describe.

4. BIGGEST MISSED OPPORTUNITY: What capability of a 1024x600 screen are we completely wasting? Be specific about what could exist in the empty space.

5. RATE: 1-10 for current state. If your 3 ideas were built, what score?`;

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
      temperature: 0.3,
      top_p: 0.9,
      top_k: 40,
      input: [
        { type: 'image', data_url: `data:image/${ext};base64,${imageBase64}` },
        { type: 'text', content: prompt },
      ],
    }),
  });
  const data = await response.json();
  if (data.error) throw new Error(JSON.stringify(data.error));
  const msg = data.output.find(o => o.type === 'message');
  const result = msg ? msg.content : JSON.stringify(data);
  console.log(result);

  const logEntry = `=== ${new Date().toISOString()} | ${screenshotPath} ===\n${result}\n\n`;
  require('fs').appendFileSync('feedback.log', logEntry);
}

const providers = { google: runGoogle, ollama: runOllama, lmstudio: runLMStudio };
(providers[provider] || runLMStudio)().catch(err => { console.error('Error:', err.message); process.exit(1); });
