# 👥 Guide for Hybrid System Setup

## 🎯 What Changed?

Your trained model is now **enhanced** with a hybrid system that:
1. ✅ **Uses YOUR model FIRST** (Aloe, Calathea, Snake, Spider plants)
2. ✅ **Falls back to Pl@ntNet API** for unknown plants (77,000+ species)
3. ✅ **Auto-captures** when plant is detected (no button needed)

**Your training work is preserved and prioritized!**

---

## 🚀 Quick Start (After Git Pull)

### Step 1: Install Node.js Dependencies

```bash
cd IOT-Project/plantnet-api
npm install
```

### Step 2: Create `.env` File

(Yan send yo na gc)

### Step 3: Run BOTH Servers

**Terminal 1 - Your Local Model:**
```bash
cd plant_cam_v4/ecodex_local
python server.py
```

You should see:
```
EcoDex server running on http://0.0.0.0:8090
Endpoints: GET /health, POST /predict
```

**Terminal 2 - Hybrid Server:**
```bash
cd plantnet-api
npm run hybrid
```

You should see:
```
🌿 EcoDex HYBRID API Server running on port 3000
🔬 Local Model: http://127.0.0.1:8090
   ✅ Local model is ONLINE 
   📚 Trained plants: Aloe Vera, Pinstripe Calathea, Snake Plant, Spider Plant
🌐 Pl@ntNet API: Configured
🎯 Strategy: Local model first, Pl@ntNet API as fallback
```

### Step 4: Upload New ESP32 Code

Use `plant_cam_plantnet.ino` (new file) instead of `plant_cam_v4.ino`

**Important:** Update line 15 with your computer's IP:
```cpp
const char* PLANTNET_API_URL = "http://192.168.1.XXX:3000/api/identify-raw";
```

**Find your IP:**
```bash
ipconfig
# Look for IPv4 Address
```

---

## 🔄 How It Works

### When You Scan a Plant:

```
📸 ESP32 detects plant → Auto-captures after 2 seconds
        ↓
🔬 Hybrid Server tries YOUR MODEL first (port 8090)
        ↓
    ┌───┴───┐
    ▼       ▼
  HIGH    LOW
  CONF    CONF
    │       │
    │       └─→ 🌐 Pl@ntNet API (fallback)
    │
    └─→ ✅ Returns YOUR model result
```

### Example 1: Snake Plant (Your Trained Model)
```
📸 Image captured
🔬 Trying local trained model...
   Local model: snake_plant (89.2% confidence)
✅ Using LOCAL MODEL result 

Serial Monitor Output:
🌿 Identified: Snake Plant
   Confidence: 89%
   Source: LOCAL MODEL
   Light: Low to bright indirect
   Water: Every 2-3 weeks
```

### Example 2: Gumamela (Not Trained)
```
📸 Image captured
🔬 Trying local trained model...
   Local model: unknown (12.3% confidence)
⚠️  Local model: Plant not in trained dataset
🔄 Trying Pl@ntNet API as fallback...
🌐 Falling back to Pl@ntNet API...
   Pl@ntNet: Hibiscus rosa-sinensis (87.4% confidence)
✅ Using PL@NTNET API result

Serial Monitor Output:
🌿 Identified: Hibiscus rosa-sinensis (Gumamela)
   Confidence: 87%
   Source: PLANTNET API
   Family: Malvaceae
```

---

## 🧪 Testing

### Test 1: Check Local Model is Running
```bash
curl http://localhost:8090/health
```

Expected:
```json
{
  "status": "ok",
  "labels": ["aloe_vera", "pinstripe_calathea", "snake_plant", "spider_plant", "unknown"],
  "threshold": 0.5
}
```

### Test 2: Check Hybrid Server is Running
```bash
curl http://localhost:3000/api/trained-plants
```

Expected:
```json
{
  "count": 4,
  "plants": [
    {"name": "Aloe Vera", "photos": 40, "trained": true},
    {"name": "Pinstripe Calathea", "photos": 82, "trained": true},
    {"name": "Snake Plant", "photos": 80, "trained": true},
    {"name": "Spider Plant", "photos": 21, "trained": true}
  ],
  "fallback": "Pl@ntNet API (77,000+ species)"
}
```

### Test 3: Web Interface
Open browser: `http://localhost:3000`

Upload a plant photo and see which system identifies it!

---

## 📊 What Happens to Your Training Work?

### Your Model is ALWAYS Tried First! ✅

| Plant Type | What Happens |
|------------|--------------|
| **Aloe Vera** | ✅ YOUR model (fast, accurate) |
| **Pinstripe Calathea** | ✅ YOUR model (fast, accurate) |
| **Snake Plant** | ✅ YOUR model (fast, accurate) |
| **Spider Plant** | ✅ YOUR model (fast, accurate) |
| **Unknown Plant** | 🔄 Tries YOUR model → Falls back to API |

**Your training work is respected and prioritized!**

---

## 🎓 Adding More Plants to Your Model

Want to train more plants? Your workflow stays the same!

### Step 1: Add Photos
```
Ecodex/dataset/new_plant_name/
  ├── 1.jpg
  ├── 2.jpg
  ├── ...
  └── 30.jpg
```

### Step 2: Retrain Model
```bash
cd plant_cam_v4/ecodex_local
.\run_train.ps1
```

### Step 3: Restart Servers
```bash
# Terminal 1
python server.py

# Terminal 2
cd ../../plantnet-api
npm run hybrid
```

### Step 4: Test!
The hybrid system will now recognize the new plant using YOUR model!

---

## 🔧 Troubleshooting

### "Local model is OFFLINE"

**Problem:** Hybrid server can't connect to your model

**Solution:**
1. Make sure `python server.py` is running in Terminal 1
2. Check port 8090 is not blocked
3. Verify model file exists: `ecodex_local/model/ecodex_baseline.npz`

**Note:** If local model is offline, hybrid server will use Pl@ntNet API only (still works!)

### ESP32 Can't Connect to Hybrid Server

**Problem:** ESP32 shows connection error

**Solutions:**
1. Check your computer's IP address hasn't changed
2. Make sure hybrid server is running (`npm run hybrid`)
3. Disable Windows Firewall temporarily to test
4. ESP32 and computer must be on same WiFi network

### Low Confidence on Your Trained Plants

**Problem:** Your model returns low confidence for a trained plant

**Solutions:**
1. Lower `CONFIDENCE_THRESHOLD` in `.env` (try 0.50)
2. Retrain model with more photos
3. Ensure test photo is similar to training data

---

## 📁 File Structure

```
IOT-Project/
├── plant_cam_v4/
│   ├── plant_cam_v4.ino          # Original (still works)
│   ├── plant_cam_plantnet.ino    # NEW: Hybrid version
│   └── ecodex_local/
│       ├── server.py              # YOUR model server
│       ├── train_baseline.py     # YOUR training script
│       └── model/
│           ├── ecodex_baseline.npz  # YOUR trained model
│           └── plant_facts.json     # YOUR plant info
├── plantnet-api/
│   ├── hybrid-server.js          # NEW: Hybrid coordinator
│   ├── package.json              # Dependencies
│   └── .env                      # Configuration (create this)
└── Ecodex/
    └── dataset/                  # YOUR training photos
        ├── aloe_vera/
        ├── pinstripe_calathea/
        ├── snake_plant/
        └── spider_plant/
```

---

## 🎯 Key Differences from Original System

| Feature | Original System | Hybrid System |
|---------|----------------|---------------|
| **Your Model** | ✅ Used | ✅ Used FIRST |
| **Unknown Plants** | ❌ Can't identify | ✅ API fallback |
| **Capture Method** | Button in browser | Auto-capture |
| **Servers Needed** | 1 (python) | 2 (python + node) |
| **ESP32 Code** | plant_cam_v4.ino | plant_cam_plantnet.ino |
| **Species Coverage** | 4 plants | 4 + 77,000+ |

---

## 💡 Pro Tips

1. **Always start local model first** - hybrid server checks for it
2. **Keep training more plants** - your model is faster than API
3. **Adjust confidence threshold** - in `.env` file (0.50 - 0.80)
4. **Monitor both terminals** - see which system is used
5. **Add plant facts** - edit `plant_facts.json` for better info

---

## 📝 Daily Workflow

### Starting Work:
```bash
# Terminal 1
cd plant_cam_v4/ecodex_local
python server.py

# Terminal 2
cd plantnet-api
npm run hybrid
```

### Testing:
1. Upload `plant_cam_plantnet.ino` to ESP32
2. Open Serial Monitor (115200 baud)
3. Point at a plant
4. Wait 2 seconds
5. See identification!

### Stopping:
- Press `Ctrl+C` in both terminals

---
## ✅ Summary

**What You Need to Do:**
1. ✅ `git pull` to get new code
2. ✅ `npm install` in plantnet-api/
3. ✅ Create `.env` file with API key
4. ✅ Run BOTH servers (python + node)
5. ✅ Upload new ESP32 code
6. ✅ Test with your trained plants!

**What Stays the Same:**
- ✅ Your training workflow
- ✅ Your model files
- ✅ Your dataset structure
- ✅ Your plant_facts.json

**What's New:**
- ✅ Hybrid server that uses your model first
- ✅ Automatic API fallback for unknown plants
- ✅ Auto-capture feature
- ✅ Support for 77,000+ species
