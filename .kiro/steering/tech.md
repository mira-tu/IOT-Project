# EcoDex Tech Stack

## Languages & Frameworks

**Python 3.12+**: Core ML pipeline (training, inference, server)
- NumPy for numerical operations and feature extraction
- Pillow (PIL) for image processing
- Standard library HTTP server (ThreadingHTTPServer)

**C++ (Arduino)**: ESP32-CAM firmware
- ESP32 Arduino Core
- esp_camera library for camera control
- WiFi and HTTP server capabilities

**JavaScript**: Frontend UI (vanilla JS, no frameworks)
- Embedded in Arduino sketch as HTML/CSS/JS

**Node.js**: Pl@ntNet API integration (hybrid mode)
- Express, Axios, FormData, Multer, CORS

**PowerShell**: Build automation and system checks

## Architecture Patterns

**Offline-First ML**: k-NN classifier with hand-crafted features (no deep learning dependencies)
- Feature extraction: RGB histograms, HSV histograms, edge detection, color statistics
- Standardization: z-score normalization with training set statistics
- Distance-weighted voting for classification
- Model serialization: NumPy .npz format

**Embedded Web Server**: ESP32 serves HTML dashboard with MJPEG streaming
- CORS-enabled for cross-origin requests
- Multipart boundary streaming for live video
- RESTful endpoints: /health, /predict, /status, /ctl

**Train-Val-Test Split**: Stratified splitting with minimum sample requirements
- Default: 70% train, 15% validation, 15% test
- Validation set used for threshold tuning
- Horizontal flip augmentation on training set only

## Project Structure

```
Ecodex/
  dataset/              # Training images organized by class folders
    aloe_vera/
    snake_plant/
    ...
  
plant_cam_v4/
  plant_cam_v4.ino      # ESP32 firmware with embedded HTML/JS
  wifi_secrets.h        # WiFi credentials (gitignored)
  ecodex_local/
    ecodex_ml.py        # Feature extraction and k-NN inference
    train_baseline.py   # Model training script
    server.py           # HTTP prediction server
    model/
      ecodex_baseline.npz         # Trained model weights
      ecodex_baseline.metrics.json # Training metrics
      plant_facts.json            # Species metadata

plantnet-api/
  .env                  # API keys (gitignored)
  express-api-server.js # Hybrid API server
  node_modules/         # Node dependencies
```

## Common Commands

### Training
```bash
# Train model from dataset
python plant_cam_v4/ecodex_local/train_baseline.py

# Train with custom parameters
python plant_cam_v4/ecodex_local/train_baseline.py --dataset Ecodex/dataset --k 7 --seed 42
```

### Running Servers
```bash
# Start local ML server (port 8090)
python plant_cam_v4/ecodex_local/server.py

# Or use PowerShell wrapper
.\plant_cam_v4\ecodex_local\run_server.ps1

# Start hybrid API server (port 3000)
cd plantnet-api
node express-api-server.js
```

### System Checks
```bash
# Verify entire system configuration
.\check_ecodex.ps1

# Check dataset, model, servers, API keys, IP addresses
```

### Arduino/ESP32
```bash
# Compile and upload (Arduino IDE or CLI)
arduino-cli compile --fqbn esp32:esp32:ai-thinker plant_cam_v4
arduino-cli upload --fqbn esp32:esp32:ai-thinker plant_cam_v4
```

## Dependencies

**Python**: No requirements.txt found, but uses:
- numpy
- pillow

**Node.js**: Install with `npm install` in plantnet-api/
- axios, express, form-data, multer, cors

**ESP32**: Managed by Arduino IDE/CLI
- ESP32 board support package
- esp_camera library (built-in)

## Configuration Files

- `wifi_secrets.h`: WiFi SSID and password for ESP32
- `.env`: Pl@ntNet API key and local model URL
- `plant_facts.json`: Species metadata (display names, care info, fun facts)
- Model hyperparameters: k=7 neighbors, threshold auto-tuned from validation set

## API Endpoints

**Local ML Server (port 8090)**:
- `GET /health` - Model status and labels
- `POST /predict` - Image classification (accepts JPEG bytes)

**Hybrid API (port 3000)**:
- `GET /health` - Service status
- `GET /api/trained-plants` - List local model labels
- `POST /api/identify` - Hybrid identification (local + Pl@ntNet)

**ESP32 Device**:
- `GET /` - Web dashboard
- `GET :81/stream` - MJPEG video stream
- `GET /status` - Telemetry JSON (greenery %, score, brightness)
- `GET /ctl` - Camera control (threshold, quality, brightness, etc.)
