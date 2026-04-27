# ðŸ‘¥ Guide for Hybrid System Setup

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
## ðŸŽ¯ What Changed?

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
Your trained model is now **enhanced** with a hybrid system that:
1. âœ… **Uses YOUR model FIRST** (Aloe, Calathea, Snake, Spider plants)
2. âœ… **Falls back to Pl@ntNet API** for unknown plants (77,000+ species)
3. âœ… **Auto-captures** when plant is detected (no button needed)

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
**Your training work is preserved and prioritized!**

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
---

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
## ðŸš€ Quick Start (After Git Pull)

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
### Step 1: Install Node.js Dependencies

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
```bash
cd IOT-Project/plantnet-api
npm install
```

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
### Step 2: Create `.env` File

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
(Yan send yo na gc)

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
### Step 3: Run BOTH Servers

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
**Terminal 1 - Your Local Model:**
```bash
cd plant_cam_v4/ecodex_local
python server.py
```

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
You should see:
```
EcoDex server running on http://0.0.0.0:8090
Endpoints: GET /health, POST /predict
```

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
**Terminal 2 - Hybrid Server:**
```bash
cd plantnet-api
npm run hybrid
```

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
You should see:
```
ðŸŒ¿ EcoDex HYBRID API Server running on port 3000
ðŸ”¬ Local Model: http://127.0.0.1:8090
   âœ… Local model is ONLINE 
   ðŸ“š Trained plants: Aloe Vera, Pinstripe Calathea, Snake Plant, Spider Plant
ðŸŒ Pl@ntNet API: Configured
ðŸŽ¯ Strategy: Local model first, Pl@ntNet API as fallback
```

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
### Step 4: Upload New ESP32 Code

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
Use `plant_cam_plantnet.ino` (new file) instead of `plant_cam_v4.ino`

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
**Important:** Update line 15 with your computer's IP:
```cpp
const char* PLANTNET_API_URL = "http://192.168.1.XXX:3000/api/identify-raw";
```

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
**Find your IP:**
```bash
ipconfig
# Look for IPv4 Address
```

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
---

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
## ðŸ”„ How It Works

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
### When You Scan a Plant:

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
```
ðŸ“¸ ESP32 detects plant â†’ Auto-captures after 2 seconds
        â†“
ðŸ”¬ Hybrid Server tries YOUR MODEL first (port 8090)
        â†“
    â”Œâ”€â”€â”€â”´â”€â”€â”€â”
    â–¼       â–¼
  HIGH    LOW
  CONF    CONF
    â”‚       â”‚
    â”‚       â””â”€â†’ ðŸŒ Pl@ntNet API (fallback)
    â”‚
    â””â”€â†’ âœ… Returns YOUR model result
```

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
### Example 1: Snake Plant (Your Trained Model)
```
ðŸ“¸ Image captured
ðŸ”¬ Trying local trained model...
   Local model: snake_plant (89.2% confidence)
âœ… Using LOCAL MODEL result 

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
Serial Monitor Output:
ðŸŒ¿ Identified: Snake Plant
   Confidence: 89%
   Source: LOCAL MODEL
   Light: Low to bright indirect
   Water: Every 2-3 weeks
```

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
### Example 2: Gumamela (Not Trained)
```
ðŸ“¸ Image captured
ðŸ”¬ Trying local trained model...
   Local model: unknown (12.3% confidence)
âš ï¸  Local model: Plant not in trained dataset
ðŸ”„ Trying Pl@ntNet API as fallback...
ðŸŒ Falling back to Pl@ntNet API...
   Pl@ntNet: Hibiscus rosa-sinensis (87.4% confidence)
âœ… Using PL@NTNET API result

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
Serial Monitor Output:
ðŸŒ¿ Identified: Hibiscus rosa-sinensis (Gumamela)
   Confidence: 87%
   Source: PLANTNET API
   Family: Malvaceae
```

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
---

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
## ðŸ§ª Testing

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
### Test 1: Check Local Model is Running
```bash
curl http://localhost:8090/health
```

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
Expected:
```json
{
  "status": "ok",
  "labels": ["aloe_vera", "pinstripe_calathea", "snake_plant", "spider_plant", "unknown"],
  "threshold": 0.5
}
```

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
### Test 2: Check Hybrid Server is Running
```bash
curl http://localhost:3000/api/trained-plants
```

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
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

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
### Test 3: Web Interface
Open browser: `http://localhost:3000`

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
Upload a plant photo and see which system identifies it!

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
---

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
## ðŸ“Š What Happens to Your Training Work?

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
### Your Model is ALWAYS Tried First! âœ…

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
| Plant Type | What Happens |
|------------|--------------|
| **Aloe Vera** | âœ… YOUR model (fast, accurate) |
| **Pinstripe Calathea** | âœ… YOUR model (fast, accurate) |
| **Snake Plant** | âœ… YOUR model (fast, accurate) |
| **Spider Plant** | âœ… YOUR model (fast, accurate) |
| **Unknown Plant** | ðŸ”„ Tries YOUR model â†’ Falls back to API |

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
**Your training work is respected and prioritized!**

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
---

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
## ðŸŽ“ Adding More Plants to Your Model

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
Want to train more plants? Your workflow stays the same!

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
### Step 1: Add Photos
```
Ecodex/dataset/new_plant_name/
  â”œâ”€â”€ 1.jpg
  â”œâ”€â”€ 2.jpg
  â”œâ”€â”€ ...
  â””â”€â”€ 30.jpg
```

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
### Step 2: Retrain Model
```bash
cd plant_cam_v4/ecodex_local
.\run_train.ps1
```

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
### Step 3: Restart Servers
```bash
# Terminal 1
python server.py

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
# Terminal 2
cd ../../plantnet-api
npm run hybrid
```

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
### Step 4: Test!
The hybrid system will now recognize the new plant using YOUR model!

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
---

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
## ðŸ”§ Troubleshooting

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
### "Local model is OFFLINE"

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
**Problem:** Hybrid server can't connect to your model

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
**Solution:**
1. Make sure `python server.py` is running in Terminal 1
2. Check port 8090 is not blocked
3. Verify model file exists: `ecodex_local/model/ecodex_baseline.npz`

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
**Note:** If local model is offline, hybrid server will use Pl@ntNet API only (still works!)

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
### ESP32 Can't Connect to Hybrid Server

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
**Problem:** ESP32 shows connection error

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
**Solutions:**
1. Check your computer's IP address hasn't changed
2. Make sure hybrid server is running (`npm run hybrid`)
3. Disable Windows Firewall temporarily to test
4. ESP32 and computer must be on same WiFi network

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
### Low Confidence on Your Trained Plants

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
**Problem:** Your model returns low confidence for a trained plant

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
**Solutions:**
1. Lower `CONFIDENCE_THRESHOLD` in `.env` (try 0.50)
2. Retrain model with more photos
3. Ensure test photo is similar to training data

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
---

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
## ðŸ“ File Structure

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
```
IOT-Project/
â”œâ”€â”€ plant_cam_v4/
â”‚   â”œâ”€â”€ plant_cam_v4.ino          # Original (still works)
â”‚   â”œâ”€â”€ plant_cam_plantnet.ino    # NEW: Hybrid version
â”‚   â””â”€â”€ ecodex_local/
â”‚       â”œâ”€â”€ server.py              # YOUR model server
â”‚       â”œâ”€â”€ train_baseline.py     # YOUR training script
â”‚       â””â”€â”€ model/
â”‚           â”œâ”€â”€ ecodex_baseline.npz  # YOUR trained model
â”‚           â””â”€â”€ plant_facts.json     # YOUR plant info
â”œâ”€â”€ plantnet-api/
â”‚   â”œâ”€â”€ hybrid-server.js          # NEW: Hybrid coordinator
â”‚   â”œâ”€â”€ package.json              # Dependencies
â”‚   â””â”€â”€ .env                      # Configuration (create this)
â””â”€â”€ Ecodex/
    â””â”€â”€ dataset/                  # YOUR training photos
        â”œâ”€â”€ aloe_vera/
        â”œâ”€â”€ pinstripe_calathea/
        â”œâ”€â”€ snake_plant/
        â””â”€â”€ spider_plant/
```

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
---

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
## ðŸŽ¯ Key Differences from Original System

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
| Feature | Original System | Hybrid System |
|---------|----------------|---------------|
| **Your Model** | âœ… Used | âœ… Used FIRST |
| **Unknown Plants** | âŒ Can't identify | âœ… API fallback |
| **Capture Method** | Button in browser | Auto-capture |
| **Servers Needed** | 1 (python) | 2 (python + node) |
| **ESP32 Code** | plant_cam_v4.ino | plant_cam_plantnet.ino |
| **Species Coverage** | 4 plants | 4 + 77,000+ |

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
---

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
## ðŸ’¡ Pro Tips

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
1. **Always start local model first** - hybrid server checks for it
2. **Keep training more plants** - your model is faster than API
3. **Adjust confidence threshold** - in `.env` file (0.50 - 0.80)
4. **Monitor both terminals** - see which system is used
5. **Add plant facts** - edit `plant_facts.json` for better info

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
---

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
## ðŸ“ Daily Workflow

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
### Starting Work:
```bash
# Terminal 1
cd plant_cam_v4/ecodex_local
python server.py

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
# Terminal 2
cd plantnet-api
npm run hybrid
```

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
### Testing:
1. Upload `plant_cam_plantnet.ino` to ESP32
2. Open Serial Monitor (115200 baud)
3. Point at a plant
4. Wait 2 seconds
5. See identification!

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
### Stopping:
- Press `Ctrl+C` in both terminals

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
---
## âœ… Summary

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
**What You Need to Do:**
1. âœ… `git pull` to get new code
2. âœ… `npm install` in plantnet-api/
3. âœ… Create `.env` file with API key
4. âœ… Run BOTH servers (python + node)
5. âœ… Upload new ESP32 code
6. âœ… Test with your trained plants!

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
**What Stays the Same:**
- âœ… Your training workflow
- âœ… Your model files
- âœ… Your dataset structure
- âœ… Your plant_facts.json

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.
**What's New:**
- âœ… Hybrid server that uses your model first
- âœ… Automatic API fallback for unknown plants
- âœ… Auto-capture feature
- âœ… Support for 77,000+ species

> Current simplified workflow: use `ECODEX_QUICK_START.md`, `start_ecodex.ps1`,
> `check_ecodex.ps1`, and `stop_ecodex.ps1` from the project root. This older
> guide explains the architecture, but some paths/config steps have since been
> simplified.

