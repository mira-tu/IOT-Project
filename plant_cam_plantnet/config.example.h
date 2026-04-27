#pragma once

// Copy this file to config.h and replace the IP with your laptop IPv4 address.
// The ESP32 and laptop must be connected to the same WiFi network.
#define ECODEX_HYBRID_API_URL "http://192.168.1.100:3000/api/identify-raw?organ=leaf"

// OTA upload settings. Change the password before a real demo.
#define ECODEX_OTA_HOSTNAME "ecodex-plant-cam"
#define ECODEX_OTA_PASSWORD "change-this-password"
