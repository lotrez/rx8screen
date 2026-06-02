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

const prompt = `You are an automotive UI designer who makes decisions. You are designing a 1024x600 dashboard for a Mazda RX-8 (rotary engine), built with LVGL on ESP32 (no images/icons/gradients — only solid rectangles, bars, arcs, and bitmap fonts).

Current layout:
- Row 0 (40%): Speed hero — 3 large DSEG7 digits (160px), gear indicator bottom-right
- Row 1 (25%): Gauge card — 3 stacked horizontal bars (water temp green, battery amber, fuel red)
- Row 2 (35%): RPM card — DSEG7 digits (64px) top-left, 30 segmented bars (white normal, amber 75%+, red 85%+)

Current palette: bg #0A0A0E, cards #111116, text #E0E4F0, dim #5A5E6A, warn #FFB700, danger #E53935

Make these decisions:

1. SPEED CARD: The speed card has lots of empty space around the 3 digits and a small gear in the corner. What should fill the empty space? Options: (a) nothing, keep it clean, (b) a subtle progress arc behind the digits showing acceleration, (c) shift the digits to be off-center to look more dynamic. Pick one. 1 sentence.

2. GAUGE BARS: The 3 horizontal bars each have their own color. Should the bar track (unfilled portion) be (a) same dark color for all (#16161A), or (b) a dimmed version of each gauge's own color? Pick one. 1 sentence.

3. RPM DIGITS: Currently left-aligned top-left. Should they be (a) left-aligned as-is, (b) centered above the bar, or (c) moved to the right side of the card? Pick one. 1 sentence.

4. TYPOGRAPHY: Should the gauge labels ("WATER TEMP", "BATTERY", "FUEL") be (a) uppercase as-is, (b) Title Case, or (c) just icons/symbols? Pick one. 1 sentence.

5. What is the single biggest flaw you see? Be specific — point to an exact element and say what's wrong. 1 sentence.

6. RATE: 1-10.`;

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
