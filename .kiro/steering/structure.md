# EcoDex Project Structure

## Directory Organization

### Root Level
- `check_ecodex.ps1` - System health check script (validates dataset, model, servers, API configuration)
- `.gitignore` - Excludes wifi_secrets.h, .env, node_modules, build artifacts

### Ecodex/
Training dataset organized by plant species. Each subfolder represents a class label.

```
Ecodex/
  dataset/
    aloe_vera/          # 40+ images
    snake_plant/        # 79 images
    pinstripe_calathea/ # 82 images
    firecracker_crossandra/
    imperial_blue/
    spider_plant/
    unknown/            # Negative class for rejection
  app/                  # Empty (future mobile app?)
  model/                # Empty (models stored in plant_cam_v4/ecodex_local/model/)
```

**Dataset Conventions**:
- Minimum 3 images per class (enforced by training script)
- Supported formats: .jpg, .jpeg, .png, .bmp, .webp
- Images auto-rotated using EXIF orientation
- Class names use snake_case (e.g., `aloe_vera`, `snake_plant`)

### plant_cam_v4/
ESP32-CAM firmware and local ML server.

```
plant_cam_v4/
  plant_cam_v4.ino           # Main Arduino sketch (1000+ lines)
                             # Includes embedded HTML/CSS/JS dashboard
  wifi_secrets.h             # WiFi credentials (gitignored)
  wifi_secrets.example.h     # Template for wifi_secrets.h
  
  ecodex_local/
    ecodex_ml.py             # Core ML: feature extraction, k-NN, model I/O
    train_baseline.py        # Training pipeline with evaluation
    server.py                # HTTP server for /predict endpoint
    run_server.ps1           # PowerShell wrapper to start server
    run_train.ps1            # PowerShell wrapper to train model
    server.out.log           # Server stdout log
    server.err.log           # Server stderr log
    
    model/
      ecodex_baseline.npz           # Trained model (NumPy archive)
      ecodex_baseline.metrics.json  # Training/validation/test metrics
      plant_facts.json              # Species metadata for UI
    
    __pycache__/           # Python bytecode cache
  
  .arduino-build/          # Arduino compilation artifacts
  .arduino-temp/           # Temporary build files
```

**Key Files**:
- `plant_cam_v4.ino`: Contains ESP32 camera initialization, telemetry logic (greenery detection, brightness analysis), HTTP handlers, and complete web UI
- `ecodex_ml.py`: Reusable ML module with functions for feature extraction, standardization, prediction, model save/load
- `train_baseline.py`: CLI tool for training with configurable dataset path, k value, random seed

### plantnet-api/
Node.js server for Pl@ntNet API integration (hybrid mode).

```
plantnet-api/
  README.md                  # Comprehensive API integration guide
  .env                       # API keys and config (gitignored)
  express-api-server.js      # Hybrid API server (local + cloud)
  nodejs-example.js          # Simple Pl@ntNet usage example
  python-example.py          # Python Pl@ntNet example
  node_modules/              # npm dependencies
  package.json               # Node.js dependencies
  package-lock.json          # Locked dependency versions
```

**Hybrid API Flow**:
1. Receives image from ESP32 or web client
2. Queries local ML server (port 8090) for offline prediction
3. Optionally queries Pl@ntNet API for cloud-based identification
4. Merges results and returns unified response

### .kiro/
Kiro AI assistant configuration.

```
.kiro/
  steering/
    product.md           # Product overview and features
    tech.md              # Tech stack and commands
    structure.md         # This file
```

## File Naming Conventions

**Python**: snake_case for modules and functions
- `ecodex_ml.py`, `train_baseline.py`, `extract_features()`

**Arduino/C++**: camelCase for functions, UPPER_CASE for constants
- `startStream()`, `PWDN_GPIO_NUM`, `PART_BOUNDARY`

**JavaScript**: camelCase for variables and functions
- `classifyFrame()`, `lastApiCheckMs`, `setDexPending()`

**Dataset Images**: Numeric or descriptive names
- `1.jpg`, `2.webp`, `ImperialBlue1.jpg`, `Firecracker_Crossandra1.jpg`

**Model Files**: Descriptive with extensions
- `ecodex_baseline.npz` (model weights)
- `ecodex_baseline.metrics.json` (evaluation results)
- `plant_facts.json` (species metadata)

## Configuration Files

**wifi_secrets.h** (ESP32):
```cpp
#define WIFI_SSID "your_network"
#define WIFI_PASSWORD "your_password"
```

**.env** (Node.js):
```
PLANTNET_API_KEY=your_api_key_here
LOCAL_MODEL_URL=http://127.0.0.1:8090
```

**plant_facts.json** (Species metadata):
```json
{
  "aloe_vera": {
    "display_name": "Aloe Vera",
    "scientific_name": "Aloe barbadensis miller",
    "about": "Succulent plant known for medicinal properties...",
    "light": "Bright indirect light",
    "water": "Low, drought-tolerant",
    "fun_fact": "Gel inside leaves soothes burns and skin irritation."
  }
}
```

## Build Artifacts

**Python**:
- `__pycache__/` - Bytecode cache (gitignored)
- `*.pyc` - Compiled Python files

**Arduino**:
- `.arduino-build/` - Compiled binaries, ELF files, partition tables
- `.arduino-temp/` - Temporary compilation files
- `*.bin`, `*.elf`, `*.map` - Firmware binaries

**Node.js**:
- `node_modules/` - npm packages (gitignored)
- `package-lock.json` - Dependency lock file

## Data Flow

1. **Training**: `Ecodex/dataset/` → `train_baseline.py` → `model/ecodex_baseline.npz`
2. **Inference**: ESP32 captures image → `server.py` loads model → extracts features → k-NN prediction → returns JSON
3. **Hybrid**: ESP32 → Node.js hybrid API → local model + Pl@ntNet → merged results → ESP32 dashboard

## Port Assignments

- **8090**: Local ML server (Python)
- **3000**: Hybrid API server (Node.js)
- **80**: ESP32 web dashboard
- **81**: ESP32 MJPEG stream

## Adding New Plant Classes

1. Create folder in `Ecodex/dataset/` with snake_case name
2. Add at least 3 images (recommended 20+ for good accuracy)
3. Run `python plant_cam_v4/ecodex_local/train_baseline.py`
4. Add entry to `plant_facts.json` with species metadata
5. Restart ML server: `.\plant_cam_v4\ecodex_local\run_server.ps1`
6. Verify with `.\check_ecodex.ps1`
