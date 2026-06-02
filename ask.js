const fs = require('fs');
const path = require('path');

const screenshotPath = process.argv[2];
const question = process.argv.slice(3).join(' ');

if (!screenshotPath || !question) {
  console.error('Usage: node ask.js <screenshot.png> <your question>');
  process.exit(1);
}

const imagePath = path.resolve(screenshotPath);
const imageBuffer = fs.readFileSync(imagePath);
const imageBase64 = imageBuffer.toString('base64');
const ext = path.extname(imagePath).slice(1);

const prompt = `You are an automotive UI designer looking at a 1024x600 dashboard for a Mazda RX-8. Answer the following question precisely, referring to what you actually see in the screenshot. Be specific — give pixel estimates if relevant. Keep your answer under 3 sentences.

Question: ${question}`;

async function run() {
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
  console.log(msg ? msg.content : JSON.stringify(data));
}

run().catch(err => { console.error('Error:', err.message); process.exit(1); });
