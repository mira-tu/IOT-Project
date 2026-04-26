# 🌿 EcoDex - Pl@ntNet API Integration Guide

Complete guide for integrating Pl@ntNet plant identification into your EcoDex app.

---

## 📋 Table of Contents
1. [Getting Started](#getting-started)
2. [API Overview](#api-overview)
3. [Code Examples](#code-examples)
4. [IoT Device Integration](#iot-device-integration)
5. [Response Format](#response-format)
6. [Common Philippine Plants](#common-philippine-plants)

---

## 🚀 Getting Started

### Step 1: Get Your FREE API Key

1. Go to **https://my.plantnet.org/**
2. Click **"Sign up"** (create free account)
3. Login and go to your dashboard
4. Copy your API key (looks like: `2a10YourAPIKey7b3c9d`)

### Step 2: Install Dependencies

**Node.js:**
```bash
npm install axios form-data express multer cors
```

**Python:**
```bash
pip install requests
```

---

## 🔌 API Overview

### Base URL
```
https://my-api.plantnet.org/v2/identify/{project}
```

### Projects (Regional Filtering)

| Project | Description | Best For |
|---------|-------------|----------|
| `useful` | Useful plants of Asia/Tropics | ⭐ **Philippines** |
| `all` | All 77,000+ species | Global |
| `k-world-flora` | World flora | General use |
| `weurope` | Western Europe | Europe only |

### Parameters

| Parameter | Required | Description |
|-----------|----------|-------------|
| `api-key` | ✅ Yes | Your API key |
| `images` | ✅ Yes | Image URL or file |
| `organs` | No | `leaf`, `flower`, `fruit`, `bark` |
| `include-related-images` | No | Get similar plant photos |

---

## 💻 Code Examples

### Node.js - Simple Example

```javascript
const axios = require('axios');
const FormData = require('form-data');
const fs = require('fs');

const API_KEY = 'YOUR_API_KEY_HERE';

async function identifyPlant(imagePath) {
  const form = new FormData();
  form.append('images', fs.createReadStream(imagePath));
  form.append('organs', 'leaf');

  const response = await axios.post(
    `https://my-api.plantnet.org/v2/identify/useful?api-key=${API_KEY}`,
    form,
    { headers: form.getHeaders() }
  );

  return response.data;
}

// Usage
identifyPlant('./plant.jpg')
  .then(data => console.log(data.results[0]))
  .catch(err => console.error(err));
```

### Python - Simple Example

```python
import requests

API_KEY = 'YOUR_API_KEY_HERE'

def identify_plant(image_path):
    url = f'https://my-api.plantnet.org/v2/identify/useful?api-key={API_KEY}'
    
    with open(image_path, 'rb') as image:
        files = {'images': image}
        data = {'organs': 'leaf'}
        response = requests.post(url, files=files, data=data)
        return response.json()

# Usage
result = identify_plant('./plant.jpg')
print(result['results'][0])
```

### cURL - Test from Command Line

```bash
curl -X POST \
  "https://my-api.plantnet.org/v2/identify/useful?api-key=YOUR_API_KEY" \
  -F "images=@plant.jpg" \
  -F "organs=leaf"
```

---

## 🤖 IoT Device Integration Flow

```
┌─────────────────┐
│  IoT Device     │
│  (ESP32/Pi)     │
│  Camera Module  │
└────────┬────────┘
         │ 1. Capture image
         │ 2. Upload to your server
         ▼
┌─────────────────┐
│  Your Backend   │
│  (Node.js/      │
│   Python)       │
└────────┬────────┘
         │ 3. Forward to Pl@ntNet
         ▼
┌─────────────────┐
│  Pl@ntNet API   │
│  (Free)         │
└────────┬────────┘
         │ 4. Return plant data
         ▼
┌─────────────────┐
│  Your Database  │
│  (Store result) │
└────────┬────────┘
         │ 5. Send to app
         ▼
┌─────────────────┐
│  EcoDex App     │
│  (PokeDex UI)   │
└─────────────────┘
```

### IoT Device Code (ESP32 Example)

```cpp
// ESP32 Arduino - Capture and upload to your server
#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_camera.h"

void captureAndIdentify() {
  camera_fb_t * fb = esp_camera_fb_get();
  
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  HTTPClient http;
  http.begin("http://your-server.com/api/identify");
  http.addHeader("Content-Type", "image/jpeg");
  
  int httpCode = http.POST(fb->buf, fb->len);
  
  if (httpCode == 200) {
    String response = http.getString();
    Serial.println("Plant identified:");
    Serial.println(response);
  }
  
  esp_camera_fb_return(fb);
  http.end();
}
```

---

## 📦 Response Format

### Raw Pl@ntNet Response

```json
{
  "query": {
    "project": "useful",
    "organs": ["leaf"]
  },
  "results": [
    {
      "score": 0.87234,
      "species": {
        "scientificNameWithoutAuthor": "Hibiscus rosa-sinensis",
        "scientificName": "Hibiscus rosa-sinensis L.",
        "commonNames": ["Gumamela", "Chinese hibiscus", "Shoe flower"],
        "family": {
          "scientificNameWithoutAuthor": "Malvaceae"
        },
        "genus": {
          "scientificNameWithoutAuthor": "Hibiscus"
        }
      },
      "images": [
        {
          "organ": "flower",
          "url": {
            "o": "https://bs.plantnet.org/image/o/abc123",
            "s": "https://bs.plantnet.org/image/s/abc123"
          }
        }
      ],
      "gbif": {
        "id": "3152559"
      }
    }
  ],
  "remainingIdentificationRequests": 498,
  "version": "2024-01-01"
}
```

### EcoDex Formatted Response

```json
{
  "success": true,
  "timestamp": "2026-04-25T10:30:00Z",
  "plants": [
    {
      "rank": 1,
      "scientificName": "Hibiscus rosa-sinensis",
      "commonNames": ["Gumamela", "Chinese hibiscus"],
      "family": "Malvaceae",
      "genus": "Hibiscus",
      "confidence": 87,
      "rarity": "Common",
      "captured": false,
      "pokedexNumber": 42,
      "images": [
        "https://bs.plantnet.org/image/o/abc123"
      ]
    }
  ],
  "meta": {
    "bestMatch": "Hibiscus rosa-sinensis",
    "bestMatchConfidence": 87,
    "remainingIdentifications": 498
  }
}
```

---

## 🇵🇭 Common Philippine Backyard Plants

Test your EcoDex with these common plants:

### Ornamentals
- **Gumamela** (Hibiscus rosa-sinensis) - Red/pink flowers
- **Santan** (Ixora coccinea) - Cluster flowers
- **Sampaguita** (Jasminum sambac) - White fragrant flowers
- **Rosal** (Rosa chinensis) - Rose

### Herbs & Spices
- **Pandan** (Pandanus amaryllifolius) - Long fragrant leaves
- **Tanglad** (Cymbopogon citratus) - Lemongrass
- **Luya** (Zingiber officinale) - Ginger
- **Dahon ng Sili** (Capsicum annuum) - Chili leaves

### Vegetables
- **Kangkong** (Ipomoea aquatica) - Water spinach
- **Malunggay** (Moringa oleifera) - Moringa
- **Kamote** (Ipomoea batatas) - Sweet potato
- **Sitaw** (Vigna unguiculata) - String beans

### Fruit Trees
- **Calamansi** (Citrus microcarpa) - Philippine lime
- **Bayabas** (Psidium guajava) - Guava
- **Mango** (Mangifera indica) - Mango
- **Papaya** (Carica papaya) - Papaya

### Native Trees
- **Narra** (Pterocarpus indicus) - National tree
- **Banaba** (Lagerstroemia speciosa) - Pride of India
- **Ilang-ilang** (Cananga odorata) - Ylang-ylang

---

## 🎮 EcoDex Game Mechanics

### Rarity System (Based on Confidence)

| Confidence | Rarity | Color |
|------------|--------|-------|
| 80-100% | Common | Gray |
| 50-79% | Uncommon | Green |
| 30-49% | Rare | Blue |
| 0-29% | Ultra Rare | Purple |

### PokeDex-Style Features

```javascript
{
  "pokedexNumber": 42,
  "scientificName": "Hibiscus rosa-sinensis",
  "commonName": "Gumamela",
  "type": "Flowering Plant",
  "habitat": "Gardens, Tropical",
  "rarity": "Common",
  "captured": true,
  "captureDate": "2026-04-25",
  "captureLocation": "Manila, Philippines",
  "stats": {
    "height": "2-5m",
    "bloomSeason": "Year-round",
    "sunlight": "Full sun"
  }
}
```

---

## 🔧 Running the Examples

### 1. Node.js Simple Example
```bash
node nodejs-example.js
```

### 2. Python Simple Example
```bash
python python-example.py
```

### 3. Express API Server
```bash
# Set your API key
export PLANTNET_API_KEY=your_key_here

# Start server
node express-api-server.js

# Test with curl
curl -X POST http://localhost:3000/api/identify \
  -F "image=@plant.jpg" \
  -F "organ=leaf"
```

---

## 📊 API Limits

| Tier | Identifications | Cost |
|------|----------------|------|
| Free | 500/day | FREE |
| Free | Unlimited* | FREE |

*Subject to fair use policy

---

## 🆘 Troubleshooting

### Error: "Invalid API key"
- Check your API key is correct
- Make sure you're logged in to my.plantnet.org

### Error: "No results found"
- Try different plant organ (leaf vs flower)
- Ensure image is clear and well-lit
- Try the 'all' project instead of 'useful'

### Low confidence scores
- Take photos in good lighting
- Focus on distinctive features (flowers, leaves)
- Avoid blurry or distant shots
- Try multiple angles

---

## 📚 Resources

- **Pl@ntNet Website:** https://my.plantnet.org/
- **API Documentation:** https://github.com/plantnet/my.plantnet
- **Community Forum:** https://identify.plantnet.org/
- **Mobile App:** https://plantnet.org/en/

---

## 🎯 Next Steps for EcoDex

1. ✅ Get Pl@ntNet API key
2. ✅ Test with sample images
3. 🔲 Build backend API server
4. 🔲 Connect IoT device
5. 🔲 Create database for captured plants
6. 🔲 Build PokeDex-style UI
7. 🔲 Add user accounts & collections
8. 🔲 Deploy to production

---

**Happy plant hunting! 🌿**
