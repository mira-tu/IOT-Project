# EcoDex Product Overview

EcoDex is a plant identification system combining embedded IoT hardware with machine learning. It's designed as a "PokeDex for plants" that can identify and catalog plant species.

## Core Components

**Hardware**: ESP32-CAM device with camera module that captures plant images and performs real-time plant detection using on-device telemetry (greenery detection, vivid color analysis, brightness tracking).

**ML Classification**: Dual-mode identification system:
- Local offline classifier using k-NN on hand-crafted image features (color histograms, edge detection, HSV analysis)
- Hybrid API integration with Pl@ntNet for cloud-based species identification

**User Interface**: Web-based dashboard served from ESP32 with live camera stream, telemetry visualization, and plant information cards displaying scientific names, care instructions, and fun facts.

## Target Use Case

Backyard plant identification for Philippine plants (gumamela, santan, malunggay, etc.) with offline-first capability. Users can train custom models on their own plant dataset or leverage Pl@ntNet's 77,000+ species database.

## Key Features

- Real-time plant detection with bounding box tracking
- Confidence-based capture triggering (only captures when view is clear)
- Offline model training and inference on laptop
- PokeDex-style plant collection and rarity system
- Adjustable camera controls and detection thresholds
