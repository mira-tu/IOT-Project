# EcoDex Quick Start

This project is a hybrid plant identification system:

1. The ESP32-CAM captures a plant image.
2. The laptop Node server receives the image on port `3000`.
3. The Node server asks the local EcoDex ML model on port `8090`.
4. If EcoDex is unsure, the Node server falls back to Pl@ntNet.
5. The ESP32 web UI shows the result and whether it came from EcoDex or Pl@ntNet.

## Daily Workflow

Open PowerShell in the project root:

```powershell
cd "path\to\IOT-Project"
.\start_ecodex.ps1
```

Leave that terminal window open while scanning plants. Then open the ESP32 IP
address in your browser and press `Capture & Identify`.

To stop the servers, press `Ctrl+C` in that terminal. You can also run:

```powershell
.\stop_ecodex.ps1
```

To diagnose problems:

```powershell
.\check_ecodex.ps1
```

## First-Time Setup

Install the Node dependencies once:

```powershell
cd "path\to\IOT-Project\plantnet-api"
npm.cmd install
```

Create `plantnet-api\.env`:

```env
PLANTNET_API_KEY=your_key_here
LOCAL_MODEL_URL=http://127.0.0.1:8090
CONFIDENCE_THRESHOLD=0.65
PORT=3000
```

Copy the ESP32 config example:

```powershell
copy .\plant_cam_plantnet\config.example.h .\plant_cam_plantnet\config.h
copy .\plant_cam_plantnet\wifi_secrets.example.h .\plant_cam_plantnet\wifi_secrets.h
```

Edit `plant_cam_plantnet\config.h` so the IP is your laptop IPv4 address:

```cpp
#define ECODEX_HYBRID_API_URL "http://YOUR-LAPTOP-IP:3000/api/identify-raw?organ=leaf"
```

Edit `plant_cam_plantnet\wifi_secrets.h` for the WiFi network used by both the
ESP32-CAM and the laptop:

```cpp
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";
```

Upload this Arduino sketch:

```text
plant_cam_plantnet\plant_cam_plantnet.ino
```

## Current Model Link

The hybrid server is linked to the trained model file:

```text
plant_cam_v4\ecodex_local\model\ecodex_baseline.npz
```

It does not automatically learn from new folders added to `Ecodex\dataset`. After adding dataset photos, retrain the model before expecting new plant labels to work.

Retrain from the project dataset:

```powershell
cd "path\to\IOT-Project\plant_cam_v4\ecodex_local"
.\run_train.ps1
```

Then restart EcoDex so the ML server reloads the new model:

```powershell
cd "path\to\IOT-Project"
.\stop_ecodex.ps1
.\start_ecodex.ps1
```

## If API Call Fails

Run:

```powershell
.\check_ecodex.ps1
```

Most failures are one of these:

- `plantnet-api\.env` is missing or has no API key.
- `.\start_ecodex.ps1` is not running.
- Laptop IP changed, so `plant_cam_plantnet\config.h` needs an update and re-upload.
- ESP32 and laptop are not on the same WiFi network.
- Windows Firewall is blocking port `3000`.
