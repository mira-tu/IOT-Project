// EcoDex Plant Cam with Pl@ntNet API Integration
// AI-Thinker ESP32-CAM → Pl@ntNet API (no training needed!)

#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "wifi_secrets.h"
#include "esp_http_server.h"
#include "img_converters.h"

// ─── WiFi (from wifi_secrets.h) ──────────────────────────────────────────────
// const char* ssid = "YOUR_WIFI_SSID";
// const char* password = "YOUR_WIFI_PASSWORD";

// ─── Pl@ntNet API Configuration ──────────────────────────────────────────────
// Your backend server running the Pl@ntNet API
const char* PLANTNET_API_URL = "http://192.168.1.100:3000/api/identify-url";
// Change to your computer's IP address where the Node.js server is running

// ─── Hardware pins ───────────────────────────────────────────────────────────
#define BUZZER_PIN 12
#define FLASH_PIN   4

// ─── Camera pins (AI-Thinker ESP32-CAM) ─────────────────────────────────────
#define PWDN_GPIO_NUM   32
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM    0
#define SIOD_GPIO_NUM   26
#define SIOC_GPIO_NUM   27
#define Y9_GPIO_NUM     35
#define Y8_GPIO_NUM     34
#define Y7_GPIO_NUM     39
#define Y6_GPIO_NUM     36
#define Y5_GPIO_NUM     21
#define Y4_GPIO_NUM     19
#define Y3_GPIO_NUM     18
#define Y2_GPIO_NUM      5
#define VSYNC_GPIO_NUM  25
#define HREF_GPIO_NUM   23
#define PCLK_GPIO_NUM   22

// ─── Detection state ─────────────────────────────────────────────────────────
volatile bool  streamActive   = false;
volatile bool  plantDetected  = false;
volatile bool  viewBlocked    = false;
volatile float lastGreenPct   = 0.0f;
volatile float lastVividPct   = 0.0f;
volatile float lastPlantScore = 0.0f;
volatile float lastBrightness = 0.0f;
volatile uint32_t lastDetectMs = 0;
volatile int   trackX = 0, trackY = 0, trackW = 0, trackH = 0;
int            plantThreshold = 18;

// ─── Plant identification state ──────────────────────────────────────────────
String lastIdentifiedPlant = "";
float lastConfidence = 0.0f;
String lastCommonName = "";
String lastFamily = "";
bool identificationInProgress = false;

// ─── Stream constants ────────────────────────────────────────────────────────
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* STREAM_CT  = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* STREAM_BND = "\r\n--" PART_BOUNDARY "\r\n";
static const char* STREAM_PT  = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t camera_httpd = NULL;
httpd_handle_t stream_httpd = NULL;

// ════════════════════════════════════════════════════════════════════════════
//  Pl@ntNet API Integration
// ════════════════════════════════════════════════════════════════════════════

// Upload image to your backend server which calls Pl@ntNet API
bool identifyPlantWithAPI(camera_fb_t *fb) {
  if (!fb || identificationInProgress) return false;
  
  identificationInProgress = true;
  Serial.println("📸 Sending image to Pl@ntNet API...");
  
  HTTPClient http;
  http.begin(PLANTNET_API_URL);
  http.addHeader("Content-Type", "image/jpeg");
  http.setTimeout(15000); // 15 second timeout
  
  // Send the JPEG image directly
  int httpCode = http.POST(fb->buf, fb->len);
  
  if (httpCode == 200) {
    String response = http.getString();
    Serial.println("✅ API Response received");
    Serial.println(response);
    
    // Parse JSON response (simple parsing)
    // Expected format: {"plants":[{"scientificName":"...","commonNames":[...],"confidence":87}]}
    
    int plantIdx = response.indexOf("\"scientificName\"");
    if (plantIdx > 0) {
      int nameStart = response.indexOf(":", plantIdx) + 2;
      int nameEnd = response.indexOf("\"", nameStart);
      lastIdentifiedPlant = response.substring(nameStart, nameEnd);
      
      // Get common name
      int commonIdx = response.indexOf("\"commonNames\"");
      if (commonIdx > 0) {
        int commonStart = response.indexOf("[", commonIdx) + 2;
        int commonEnd = response.indexOf("\"", commonStart);
        if (commonEnd > commonStart) {
          lastCommonName = response.substring(commonStart, commonEnd);
        }
      }
      
      // Get confidence
      int confIdx = response.indexOf("\"confidence\"");
      if (confIdx > 0) {
        int confStart = response.indexOf(":", confIdx) + 1;
        int confEnd = response.indexOf(",", confStart);
        if (confEnd < 0) confEnd = response.indexOf("}", confStart);
        String confStr = response.substring(confStart, confEnd);
        lastConfidence = confStr.toFloat();
      }
      
      // Get family
      int famIdx = response.indexOf("\"family\"");
      if (famIdx > 0) {
        int famStart = response.indexOf(":", famIdx) + 2;
        int famEnd = response.indexOf("\"", famStart);
        if (famEnd > famStart) {
          lastFamily = response.substring(famStart, famEnd);
        }
      }
      
      Serial.printf("🌿 Identified: %s (%s)\n", lastIdentifiedPlant.c_str(), lastCommonName.c_str());
      Serial.printf("   Confidence: %.0f%%\n", lastConfidence);
      Serial.printf("   Family: %s\n", lastFamily.c_str());
      
      // Success beep pattern
      for (int i = 0; i < 3; i++) {
        digitalWrite(FLASH_PIN, HIGH);
        ledcAttach(BUZZER_PIN, 2500, 8);
        ledcWrite(BUZZER_PIN, 150);
        delay(100);
        digitalWrite(FLASH_PIN, LOW);
        ledcWrite(BUZZER_PIN, 0);
        delay(100);
      }
      
      http.end();
      identificationInProgress = false;
      return true;
    }
  } else {
    Serial.printf("❌ API Error: %d\n", httpCode);
    if (httpCode > 0) {
      Serial.println(http.getString());
    }
  }
  
  http.end();
  identificationInProgress = false;
  return false;
}

// Auto-capture when plant is detected and stable
void autoCapturePlant() {
  static uint32_t stableDetectionStart = 0;
  static uint32_t lastAutoCapture = 0;
  const uint32_t STABLE_DURATION = 2000; // 2 seconds stable
  const uint32_t CAPTURE_COOLDOWN = 10000; // 10 seconds between auto-captures
  
  uint32_t now = millis();
  
  if (plantDetected && !viewBlocked && !identificationInProgress) {
    // Plant is detected and view is clear
    if (stableDetectionStart == 0) {
      stableDetectionStart = now;
    } else if ((now - stableDetectionStart) >= STABLE_DURATION) {
      // Plant has been stable for 2 seconds
      if ((now - lastAutoCapture) >= CAPTURE_COOLDOWN) {
        Serial.println("🎯 Auto-capturing stable plant...");
        
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb) {
          identifyPlantWithAPI(fb);
          esp_camera_fb_return(fb);
          lastAutoCapture = now;
        }
        stableDetectionStart = 0;
      }
    }
  } else {
    // Reset stable detection timer
    stableDetectionStart = 0;
  }
}

// ════════════════════════════════════════════════════════════════════════════
//  DETECTION (same as original)
// ════════════════════════════════════════════════════════════════════════════

#define DET_W 320
#define DET_H 240

void runDetection() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return;

  const int detStep = 4;
  const int detW = fb->width;
  const int detH = fb->height;
  const size_t rgbLen = (size_t)detW * detH * 3;
  uint8_t *rgb = (uint8_t *)(psramFound() ? ps_malloc(rgbLen) : malloc(rgbLen));
  bool ok = rgb && fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, rgb);
  esp_camera_fb_return(fb);

  if (!ok || !rgb) {
    if (rgb) free(rgb);
    return;
  }

  long greenPx = 0, vividPx = 0, totalPx = 0, darkPx = 0, blockedPx = 0, sumBright = 0;
  int  minX = detW, maxX = 0, minY = detH, maxY = 0;

  for (int y = 0; y < detH; y += detStep) {
    for (int x = 0; x < detW; x += detStep) {
      int i = (y * detW + x) * 3;
      uint8_t r = rgb[i];
      uint8_t g = rgb[i + 1];
      uint8_t b = rgb[i + 2];

      int bright = (r + g + b) / 3;
      int maxC   = max(r, max(g, b));
      int minC   = min(r, min(g, b));
      int sat    = (maxC > 0) ? ((maxC - minC) * 255 / maxC) : 0;

      sumBright += bright;
      totalPx++;

      if (bright < 45) darkPx++;
      if (bright > 175 && sat < 22) blockedPx++;

      int greenLead = (int)g - (((int)r + (int)b) / 2);
      bool greenish = (g > 38) && (greenLead > 8)  && (sat > 22);
      bool vivid    = (g > 52) && (greenLead > 18) && (sat > 40);

      if (greenish) {
        greenPx++;
        if (x < minX) minX = x;
        if (x > maxX) maxX = x;
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;
      }
      if (vivid) vividPx++;
    }
  }

  free(rgb);
  if (totalPx == 0) return;

  float gPct      = (greenPx   * 100.0f) / totalPx;
  float vividPct  = (vividPx   * 100.0f) / totalPx;
  float darkPct   = (darkPx    * 100.0f) / totalPx;
  float blockPct  = (blockedPx * 100.0f) / totalPx;
  float avgBright = (float)sumBright / totalPx;
  float plantScore = (gPct * 0.63f) + (vividPct * 0.37f);

  lastGreenPct   = gPct;
  lastVividPct   = vividPct;
  lastBrightness = avgBright;
  lastDetectMs   = millis();

  viewBlocked = (darkPct > 65.0f) || (blockPct > 70.0f);
  if (viewBlocked) plantScore = 0.0f;
  lastPlantScore = plantScore;

  if (greenPx > 8 && maxX > minX && maxY > minY) {
    int bx = max(0,       (minX * 100 / detW) - 4);
    int by = max(0,       (minY * 100 / detH) - 4);
    int bw = min(100 - bx, ((maxX - minX) * 100 / detW) + 8);
    int bh = min(100 - by, ((maxY - minY) * 100 / detH) + 8);
    trackX = (trackX * 2 + bx) / 3;
    trackY = (trackY * 2 + by) / 3;
    trackW = (trackW * 2 + bw) / 3;
    trackH = (trackH * 2 + bh) / 3;
  } else {
    trackX = trackY = trackW = trackH = 0;
  }

  bool prev = plantDetected;
  plantDetected = (plantScore >= plantThreshold) && !viewBlocked;

  if (viewBlocked) {
    Serial.printf("BLOCKED  bright=%.0f  dark=%.1f%%  close=%.1f%%\n",
                  avgBright, darkPct, blockPct);
  } else if (plantDetected && !prev) {
    Serial.printf("PLANT    green=%.1f%%  box=%d,%d %d×%d\n",
                  gPct, (int)trackX, (int)trackY, (int)trackW, (int)trackH);
    for (int i = 0; i < 2; i++) {
      digitalWrite(FLASH_PIN, HIGH); delay(80);
      digitalWrite(FLASH_PIN, LOW);  delay(80);
    }
  }

  if (plantDetected && !viewBlocked) {
    ledcAttach(BUZZER_PIN, 2000, 8);
    ledcWrite(BUZZER_PIN, 100);
  } else {
    ledcWrite(BUZZER_PIN, 0);
  }
}

void detectionTask(void *param) {
  for (;;) {
    if (streamActive) {
      runDetection();
      autoCapturePlant(); // Auto-capture when stable
      vTaskDelay(pdMS_TO_TICKS(1000));
    } else {
      vTaskDelay(pdMS_TO_TICKS(200));
    }
  }
}

// ════════════════════════════════════════════════════════════════════════════
//  HTTP HANDLERS
// ════════════════════════════════════════════════════════════════════════════

static esp_err_t stream_handler(httpd_req_t *req) {
  char part_buf[64];
  httpd_resp_set_type(req, STREAM_CT);
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  while (true) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) break;

    size_t hlen = snprintf(part_buf, 64, STREAM_PT, fb->len);
    esp_err_t res = httpd_resp_send_chunk(req, part_buf, hlen);
    if (res == ESP_OK)
      res = httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len);
    if (res == ESP_OK)
      res = httpd_resp_send_chunk(req, STREAM_BND, strlen(STREAM_BND));

    esp_camera_fb_return(fb);
    if (res != ESP_OK) break;
  }
  return ESP_OK;
}

static esp_err_t status_handler(httpd_req_t *req) {
  char json[512];
  uint32_t ageMs = lastDetectMs ? (millis() - lastDetectMs) : 0;
  
  // Escape quotes in plant names for JSON
  String safePlantName = lastIdentifiedPlant;
  safePlantName.replace("\"", "\\\"");
  String safeCommonName = lastCommonName;
  safeCommonName.replace("\"", "\\\"");
  String safeFamily = lastFamily;
  safeFamily.replace("\"", "\\\"");
  
  snprintf(json, sizeof(json),
    "{\"detected\":%s,\"green\":%.1f,\"vivid\":%.1f,"
    "\"score\":%.1f,\"blocked\":%s,\"brightness\":%.1f,\"threshold\":%d,"
    "\"age\":%lu,\"tx\":%d,\"ty\":%d,\"tw\":%d,\"th\":%d,"
    "\"plant\":\"%s\",\"commonName\":\"%s\",\"family\":\"%s\",\"confidence\":%.0f}",
    plantDetected ? "true"  : "false",
    (float)lastGreenPct,
    (float)lastVividPct,
    (float)lastPlantScore,
    viewBlocked   ? "true"  : "false",
    (float)lastBrightness,
    plantThreshold,
    (unsigned long)ageMs,
    (int)trackX, (int)trackY, (int)trackW, (int)trackH,
    safePlantName.c_str(),
    safeCommonName.c_str(),
    safeFamily.c_str(),
    lastConfidence);

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, json, strlen(json));
}

// Manual capture endpoint
static esp_err_t capture_handler(httpd_req_t *req) {
  if (identificationInProgress) {
    httpd_resp_send(req, "{\"error\":\"Identification in progress\"}", 37);
    return ESP_OK;
  }
  
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    httpd_resp_send(req, "{\"error\":\"Camera capture failed\"}", 34);
    return ESP_OK;
  }
  
  bool success = identifyPlantWithAPI(fb);
  esp_camera_fb_return(fb);
  
  if (success) {
    httpd_resp_send(req, "{\"success\":true}", 16);
  } else {
    httpd_resp_send(req, "{\"error\":\"API call failed\"}", 28);
  }
  
  return ESP_OK;
}

static esp_err_t ctl_handler(httpd_req_t *req) {
  char buf[64];
  httpd_req_get_url_query_str(req, buf, sizeof(buf));
  char var[32], val[16];

  if (httpd_query_key_value(buf, "var", var, sizeof(var)) == ESP_OK &&
      httpd_query_key_value(buf, "val", val, sizeof(val)) == ESP_OK) {
    int v = atoi(val);
    sensor_t *s = esp_camera_sensor_get();

    if      (!strcmp(var, "framesize"))  s->set_framesize(s, (framesize_t)v);
    else if (!strcmp(var, "quality"))    s->set_quality(s, v);
    else if (!strcmp(var, "contrast"))   s->set_contrast(s, v);
    else if (!strcmp(var, "brightness")) s->set_brightness(s, v);
    else if (!strcmp(var, "saturation")) s->set_saturation(s, v);
    else if (!strcmp(var, "vflip"))      s->set_vflip(s, v);
    else if (!strcmp(var, "hmirror"))    s->set_hmirror(s, v);
    else if (!strcmp(var, "awb"))        s->set_whitebal(s, v);
    else if (!strcmp(var, "threshold"))  plantThreshold = v;
    else if (!strcmp(var, "stream")) {
      streamActive = (v == 1);
      if (!streamActive) {
        plantDetected = false;
        viewBlocked   = false;
        lastGreenPct = lastVividPct = lastPlantScore = 0.0f;
        lastBrightness = 0.0f;
        lastDetectMs = 0;
        trackX = trackY = trackW = trackH = 0;
        ledcWrite(BUZZER_PIN, 0);
      }
    }
  }
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, "OK", 2);
}

static esp_err_t index_handler(httpd_req_t *req) {
  const char* html = "<html><body><h1>EcoDex Plant Cam</h1>"
    "<p>Plant Detection: Active</p>"
    "<p>API: Pl@ntNet</p>"
    "<p><a href='/stream'>View Stream</a></p>"
    "<p><a href='/capture'>Capture & Identify</a></p>"
    "</body></html>";
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, html, strlen(html));
}

// ════════════════════════════════════════════════════════════════════════════
//  SETUP
// ════════════════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(FLASH_PIN,  OUTPUT);
  digitalWrite(FLASH_PIN, LOW);

  // Camera config
  camera_config_t cfg = {};
  cfg.ledc_channel = LEDC_CHANNEL_0;
  cfg.ledc_timer   = LEDC_TIMER_0;
  cfg.pin_d0 = Y2_GPIO_NUM; cfg.pin_d1 = Y3_GPIO_NUM;
  cfg.pin_d2 = Y4_GPIO_NUM; cfg.pin_d3 = Y5_GPIO_NUM;
  cfg.pin_d4 = Y6_GPIO_NUM; cfg.pin_d5 = Y7_GPIO_NUM;
  cfg.pin_d6 = Y8_GPIO_NUM; cfg.pin_d7 = Y9_GPIO_NUM;
  cfg.pin_xclk     = XCLK_GPIO_NUM;
  cfg.pin_pclk     = PCLK_GPIO_NUM;
  cfg.pin_vsync    = VSYNC_GPIO_NUM;
  cfg.pin_href     = HREF_GPIO_NUM;
  cfg.pin_sccb_sda = SIOD_GPIO_NUM;
  cfg.pin_sccb_scl = SIOC_GPIO_NUM;
  cfg.pin_pwdn     = PWDN_GPIO_NUM;
  cfg.pin_reset    = RESET_GPIO_NUM;
  cfg.xclk_freq_hz = 20000000;
  cfg.pixel_format = PIXFORMAT_JPEG;
  cfg.jpeg_quality = 18;
  cfg.fb_count     = 2;
  cfg.grab_mode    = CAMERA_GRAB_LATEST;

  if (psramFound()) {
    cfg.frame_size  = FRAMESIZE_QVGA;
    cfg.fb_location = CAMERA_FB_IN_PSRAM;
  } else {
    cfg.frame_size  = FRAMESIZE_QVGA;
    cfg.fb_location = CAMERA_FB_IN_DRAM;
    cfg.fb_count    = 1;
  }

  if (esp_camera_init(&cfg) != ESP_OK) {
    Serial.println("Camera init FAILED");
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_QVGA);
  s->set_quality(s, 18);
  s->set_vflip(s,    1);
  s->set_hmirror(s,  1);
  s->set_whitebal(s, 1);
  s->set_awb_gain(s, 1);

  // WiFi
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(400); Serial.print("."); }
  Serial.println("\n🌿 EcoDex Ready!");
  Serial.println("IP: http://" + WiFi.localIP().toString());
  Serial.printf("API: %s\n", PLANTNET_API_URL);

  // HTTP server
  httpd_config_t hcfg  = HTTPD_DEFAULT_CONFIG();
  hcfg.server_port     = 80;
  hcfg.max_uri_handlers = 10;
  hcfg.stack_size      = 8192;

  httpd_config_t scfg  = hcfg;
  scfg.server_port     = 81;
  scfg.ctrl_port       = hcfg.ctrl_port + 1;

  httpd_uri_t uIndex   = {"/",        HTTP_GET, index_handler,   nullptr};
  httpd_uri_t uStatus  = {"/status",  HTTP_GET, status_handler,  nullptr};
  httpd_uri_t uCtl     = {"/ctl",     HTTP_GET, ctl_handler,     nullptr};
  httpd_uri_t uCapture = {"/capture", HTTP_GET, capture_handler, nullptr};
  httpd_uri_t uStream  = {"/stream",  HTTP_GET, stream_handler,  nullptr};

  if (httpd_start(&camera_httpd, &hcfg) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &uIndex);
    httpd_register_uri_handler(camera_httpd, &uStatus);
    httpd_register_uri_handler(camera_httpd, &uCtl);
    httpd_register_uri_handler(camera_httpd, &uCapture);
    Serial.println("UI server started on port 80");
  }

  if (httpd_start(&stream_httpd, &scfg) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &uStream);
    Serial.println("Stream server started on port 81");
  }

  xTaskCreatePinnedToCore(detectionTask, "detect", 16384, nullptr, 1, nullptr, 0);
  
  Serial.println("\n✅ EcoDex Plant Cam with Pl@ntNet API ready!");
  Serial.println("📸 Point camera at a plant - it will auto-identify after 2 seconds");
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
