// EcoDex Plant Cam with Pl@ntNet API integration
// AI-Thinker ESP32-CAM -> EcoDex hybrid API

#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoOTA.h>
#include "wifi_secrets.h"
#include "config.h"
#include "esp_http_server.h"
#include "img_converters.h"

bool identifyPlantWithAPI(camera_fb_t *fb);
bool identifyImageWithAPI(const uint8_t *imageData, size_t imageLen);
static esp_err_t stream_handler(httpd_req_t *req);
static esp_err_t status_handler(httpd_req_t *req);
static esp_err_t capture_handler(httpd_req_t *req);
static esp_err_t last_capture_handler(httpd_req_t *req);
static esp_err_t ctl_handler(httpd_req_t *req);
static esp_err_t index_handler(httpd_req_t *req);
bool captureAndIdentifyFrame();
void handlePhysicalButtons();
void stopStreamState();

// WiFi settings come from wifi_secrets.h
// const char* ssid = "YOUR_WIFI_SSID";
// const char* password = "YOUR_WIFI_PASSWORD";

// Hybrid API configuration
// Your laptop's hybrid API server. It tries EcoDex ML first, then Pl@ntNet.
const char* PLANTNET_API_URL = ECODEX_HYBRID_API_URL;
const char* OTA_HOSTNAME = ECODEX_OTA_HOSTNAME;
const char* OTA_PASSWORD = ECODEX_OTA_PASSWORD;

// Hardware pins
#define FLASH_PIN   4
#define BUTTON_STREAM_PIN  13
#define BUTTON_CAPTURE_PIN 14
#define BUTTON_CLEAR_PIN   15

// Camera pins for AI-Thinker ESP32-CAM
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

// Plant detection state
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
static const bool AUTO_CAPTURE_ENABLED = false;
static const uint32_t STREAM_DETECT_INTERVAL_MS = 1600;
static const uint32_t STREAM_FRAME_DELAY_MS = 65;
static const int DEFAULT_JPEG_QUALITY = 24;

// Plant identification state
String lastIdentifiedPlant = "";
float lastConfidence = 0.0f;
String lastCommonName = "";
String lastFamily = "";
String lastSource = "";
String lastAbout = "";
String lastLight = "";
String lastWater = "";
String lastHealthStatus = "";
String lastHealthNote = "";
int lastHealthConfidence = 0;
String lastApiError = "";
int lastApiStatus = 0;
bool identificationInProgress = false;
uint8_t *lastCaptureJpeg = nullptr;
size_t lastCaptureLen = 0;
String selectedOrgan = "leaf";
volatile uint32_t clearRevision = 0;

// MJPEG stream constants
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* STREAM_CT  = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* STREAM_BND = "\r\n--" PART_BOUNDARY "\r\n";
static const char* STREAM_PT  = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t camera_httpd = NULL;
httpd_handle_t stream_httpd = NULL;

String jsonStringValue(const String& response, const char* key) {
  String token = "\"";
  token += key;
  token += "\"";

  int keyIdx = response.indexOf(token);
  if (keyIdx < 0) return "";

  int colon = response.indexOf(":", keyIdx);
  if (colon < 0) return "";

  int firstQuote = response.indexOf("\"", colon + 1);
  if (firstQuote < 0) return "";

  int secondQuote = response.indexOf("\"", firstQuote + 1);
  if (secondQuote < 0) return "";

  return response.substring(firstQuote + 1, secondQuote);
}

float jsonFloatValue(const String& response, const char* key) {
  String token = "\"";
  token += key;
  token += "\"";

  int keyIdx = response.indexOf(token);
  if (keyIdx < 0) return 0.0f;

  int colon = response.indexOf(":", keyIdx);
  if (colon < 0) return 0.0f;

  int valueStart = colon + 1;
  while (valueStart < response.length() && response.charAt(valueStart) == ' ') {
    valueStart++;
  }

  int valueEnd = response.indexOf(",", valueStart);
  if (valueEnd < 0) valueEnd = response.indexOf("}", valueStart);
  if (valueEnd < 0) return 0.0f;

  return response.substring(valueStart, valueEnd).toFloat();
}

String jsonEscape(String value) {
  value.replace("\\", "\\\\");
  value.replace("\"", "\\\"");
  value.replace("\r", " ");
  value.replace("\n", " ");
  return value;
}

void clearIdentityState() {
  clearRevision++;
  lastIdentifiedPlant = "";
  lastCommonName = "";
  lastFamily = "";
  lastSource = "";
  lastAbout = "";
  lastLight = "";
  lastWater = "";
  lastHealthStatus = "";
  lastHealthNote = "";
  lastHealthConfidence = 0;
  lastConfidence = 0.0f;
  lastApiError = "";
  lastApiStatus = 0;
  if (lastCaptureJpeg) {
    free(lastCaptureJpeg);
    lastCaptureJpeg = nullptr;
  }
  lastCaptureLen = 0;
}

void saveLastCapture(const uint8_t *imageData, size_t imageLen) {
  if (!imageData || imageLen == 0) return;
  if (lastCaptureJpeg) {
    free(lastCaptureJpeg);
    lastCaptureJpeg = nullptr;
    lastCaptureLen = 0;
  }
  uint8_t *copy = (uint8_t *)(psramFound() ? ps_malloc(imageLen) : malloc(imageLen));
  if (!copy) return;
  memcpy(copy, imageData, imageLen);
  lastCaptureJpeg = copy;
  lastCaptureLen = imageLen;
}

void updateVisualHealthEstimate() {
  if (viewBlocked) {
    lastHealthStatus = "Image unclear";
    lastHealthNote = "The view is blocked, too dark, too bright, or too close. Retake the photo with clearer lighting.";
    lastHealthConfidence = 30;
    return;
  }

  if (!plantDetected) {
    lastHealthStatus = "No visible plant";
    lastHealthNote = "EcoDex needs a clear view of leaves or flowers before estimating plant health.";
    lastHealthConfidence = 20;
    return;
  }

  if (lastBrightness < 55.0f) {
    lastHealthStatus = "Image too dark";
    lastHealthNote = "The plant may be healthy, but the photo is too dark for a reliable visual health check.";
    lastHealthConfidence = 35;
    return;
  }

  if (lastBrightness > 190.0f) {
    lastHealthStatus = "Image too bright";
    lastHealthNote = "The image is overexposed, so pale or damaged areas may be hidden.";
    lastHealthConfidence = 35;
    return;
  }

  if (lastGreenPct >= 42.0f && lastPlantScore >= 32.0f) {
    lastHealthStatus = "Healthy-looking";
    lastHealthNote = "The visible plant area has a strong green signal and balanced brightness. This is only a visual screening, not a full diagnosis.";
    lastHealthConfidence = 75;
    return;
  }

  if (lastGreenPct < 22.0f || lastVividPct < 8.0f) {
    lastHealthStatus = "Possible stress";
    lastHealthNote = "The image shows weak green or vivid color. This may indicate discoloration, dryness, damage, or poor lighting.";
    lastHealthConfidence = 60;
    return;
  }

  lastHealthStatus = "Needs closer check";
  lastHealthNote = "The visible signals are mixed. Take a closer photo of the leaf or flower area to check for spots, yellowing, wilting, or pests.";
  lastHealthConfidence = 50;
}

static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<!doctype html><html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>EcoDex PlantNet Cam</title>
<style>
:root{
  --soil:#302219;
  --moss:#31543d;
  --leaf:#4f8f52;
  --sprout:#a8d46f;
  --cream:#fff6df;
  --paper:rgba(255,250,234,.88);
  --ink:#172216;
  --muted:#6d765d;
  --line:rgba(48,34,25,.16);
  --warn:#b9503f;
  --shadow:0 22px 60px rgba(28,43,22,.24);
}
*{box-sizing:border-box}
body{
  min-height:100vh;
  margin:0;
  color:var(--ink);
  font-family:Georgia,"Trebuchet MS",serif;
  background:
    radial-gradient(circle at 14% 12%, rgba(168,212,111,.45), transparent 24rem),
    radial-gradient(circle at 84% 18%, rgba(242,184,93,.34), transparent 20rem),
    linear-gradient(135deg,#e8f0cf 0%,#f8ecd0 44%,#d2dfb7 100%);
}
body:before{
  content:"";
  position:fixed;
  inset:0;
  pointer-events:none;
  opacity:.22;
  background-image:
    linear-gradient(30deg, rgba(49,84,61,.16) 12%, transparent 12.5%, transparent 87%, rgba(49,84,61,.16) 87.5%, rgba(49,84,61,.16)),
    linear-gradient(150deg, rgba(49,84,61,.16) 12%, transparent 12.5%, transparent 87%, rgba(49,84,61,.16) 87.5%, rgba(49,84,61,.16));
  background-size:42px 72px;
}
.shell{position:relative;max-width:1180px;margin:0 auto;padding:22px}
.hero{display:grid;grid-template-columns:1.1fr .9fr;gap:18px;align-items:stretch}
.panel{border:1px solid var(--line);border-radius:30px;background:var(--paper);box-shadow:var(--shadow);backdrop-filter:blur(10px)}
.intro{padding:30px;overflow:hidden;position:relative}
.intro:after{content:"";position:absolute;right:-90px;bottom:-120px;width:280px;height:280px;border-radius:50%;background:radial-gradient(circle,rgba(79,143,82,.32),transparent 68%)}
.eyebrow{display:inline-flex;gap:8px;align-items:center;padding:8px 12px;border-radius:999px;color:#f8f1dc;background:linear-gradient(135deg,var(--moss),#17291e);font:700 12px/1 "Trebuchet MS",sans-serif;letter-spacing:.14em;text-transform:uppercase}
h1{position:relative;margin:18px 0 10px;font-size:clamp(42px,8vw,86px);line-height:.88;letter-spacing:-.07em;color:var(--soil)}
.sub{position:relative;max-width:620px;margin:0;color:#4f5f43;font:18px/1.45 "Trebuchet MS",sans-serif}
.actions{display:flex;flex-wrap:wrap;gap:12px;margin-top:24px}
button,.button{border:0;border-radius:999px;padding:13px 18px;cursor:pointer;color:#fff8e8;background:linear-gradient(135deg,var(--leaf),var(--moss));box-shadow:0 12px 24px rgba(49,84,61,.2);font:800 14px/1 "Trebuchet MS",sans-serif;letter-spacing:.02em;text-decoration:none}
button.secondary{color:var(--moss);background:#fff6df;border:1px solid var(--line);box-shadow:none}
button:disabled{opacity:.55;cursor:not-allowed}
.badge-row{display:flex;flex-wrap:wrap;gap:8px;margin-top:22px}
.badge{padding:9px 11px;border-radius:999px;background:rgba(255,255,255,.62);border:1px solid var(--line);color:var(--moss);font:800 12px/1 "Trebuchet MS",sans-serif}
.badge.warn{color:var(--warn)}
.camera-card{padding:16px}
.stage{position:relative;overflow:hidden;min-height:390px;border-radius:22px;background:linear-gradient(135deg,#152016,#2e422e)}
#stream{display:none;width:100%;height:100%;min-height:390px;object-fit:cover}
.empty{position:absolute;inset:0;display:grid;place-items:center;padding:28px;color:#f7eed7;text-align:center}
.empty strong{display:block;font-size:28px;margin-bottom:8px}
.empty span{font:15px/1.45 "Trebuchet MS",sans-serif;color:#d9e7c2}
.reticle{position:absolute;display:none;border:3px solid var(--sprout);border-radius:20px;box-shadow:0 0 0 999px rgba(0,0,0,.18),0 0 24px rgba(168,212,111,.8);transition:all .35s ease}
.grid{display:grid;grid-template-columns:1.05fr .95fr;gap:18px;margin-top:18px}
.card{padding:22px}
.card-head{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:14px}
.card h2{margin:0;color:var(--soil);font-size:28px;letter-spacing:-.03em}
button.clear-btn{padding:10px 13px;color:var(--moss);background:#fff6df;border:1px solid var(--line);box-shadow:none}
.plant-name{min-height:38px;font-size:32px;line-height:1;font-weight:900;letter-spacing:-.04em;color:var(--moss)}
.common{margin-top:8px;color:var(--muted);font:16px/1.4 "Trebuchet MS",sans-serif}
.identity-media{display:none;margin-top:16px;grid-template-columns:150px 1fr;gap:14px;align-items:start}
.identity-media img{width:150px;aspect-ratio:4/3;object-fit:cover;border-radius:16px;border:1px solid var(--line);background:rgba(49,84,61,.12)}
.plant-details{display:grid;gap:8px;color:#44533c;font:14px/1.45 "Trebuchet MS",sans-serif}
.plant-details b{color:var(--soil)}
.result-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;margin-top:18px}
.metric{padding:14px;border-radius:20px;background:rgba(255,255,255,.6);border:1px solid var(--line)}
.metric small{display:block;color:var(--muted);font:800 11px/1 "Trebuchet MS",sans-serif;text-transform:uppercase;letter-spacing:.1em}
.metric b{display:block;margin-top:8px;font-size:24px;color:var(--soil)}
.meter{height:12px;overflow:hidden;border-radius:999px;background:rgba(49,84,61,.13);margin-top:8px}
.fill{height:100%;width:0;background:linear-gradient(90deg,var(--leaf),var(--sprout));transition:width .35s ease}
.controls{display:grid;gap:16px}
label{display:grid;gap:8px;color:var(--muted);font:800 12px/1 "Trebuchet MS",sans-serif;text-transform:uppercase;letter-spacing:.09em}
input[type=range]{width:100%;accent-color:var(--leaf)}
select{width:100%;border:1px solid var(--line);border-radius:14px;background:rgba(255,255,255,.68);color:var(--moss);padding:11px 12px;font:800 14px/1 "Trebuchet MS",sans-serif}
.log{min-height:58px;padding:14px;border-radius:18px;color:#41513b;background:rgba(255,255,255,.58);border:1px dashed rgba(49,84,61,.25);font:14px/1.45 "Trebuchet MS",sans-serif}
@media(max-width:860px){.hero,.grid{grid-template-columns:1fr}.shell{padding:12px}.result-grid{grid-template-columns:1fr}.identity-media{grid-template-columns:1fr}.identity-media img{width:100%}#stream,.stage{min-height:280px}}
</style>
</head>
<body>
<main class="shell">
  <section class="hero">
    <div class="panel intro">
      <div class="eyebrow">EcoDex PlantNet mode</div>
      <h1>Plant Cam</h1>
      <p class="sub">Live ESP32-CAM plant detection with Pl@ntNet identification. Start the stream, center the leaves, then capture when the signal looks steady.</p>
      <div class="actions">
        <button id="startBtn" onclick="startStream()">Start Stream</button>
        <button id="stopBtn" class="secondary" onclick="stopStream()" style="display:none">Stop Stream</button>
        <button id="captureBtn" onclick="capturePlant()" disabled>Capture & Identify</button>
      </div>
      <div class="badge-row">
        <span id="detectBadge" class="badge">Detection idle</span>
        <span id="blockBadge" class="badge">View clear</span>
        <span id="apiBadge" class="badge">Hybrid ready</span>
      </div>
    </div>
    <div class="panel camera-card">
      <div class="stage">
        <img id="stream" alt="Plant camera stream">
        <div id="empty" class="empty"><div><strong>No stream yet</strong><span>Tap Start Stream to wake the camera preview.</span></div></div>
        <div id="reticle" class="reticle"></div>
      </div>
    </div>
  </section>
  <section class="grid">
    <div class="panel card">
      <div class="card-head">
        <h2>Plant Identity</h2>
        <button class="clear-btn" onclick="clearIdentity()">Clear</button>
      </div>
      <div id="plantName" class="plant-name">Waiting for capture</div>
      <div id="plantMeta" class="common">No identification result yet.</div>
      <div id="identityMedia" class="identity-media">
        <img id="capturePreview" alt="Captured plant photo">
        <div id="plantDetails" class="plant-details"></div>
      </div>
      <div class="result-grid">
        <div class="metric"><small>Confidence</small><b id="confidence">--%</b><div class="meter"><div id="confidenceFill" class="fill"></div></div></div>
        <div class="metric"><small>Plant Score</small><b id="score">0.0</b><div class="meter"><div id="scoreFill" class="fill"></div></div></div>
        <div class="metric"><small>Green Signal</small><b id="green">0.0%</b><div class="meter"><div id="greenFill" class="fill"></div></div></div>
      </div>
    </div>
    <div class="panel card controls">
      <h2>Camera Tuning</h2>
      <label>Detection Threshold <span id="thresholdVal">18</span><input id="threshold" type="range" min="4" max="55" value="18" oninput="setThreshold(this.value)"></label>
      <label>Brightness <span id="brightVal">0</span><input id="cam_brightness" type="range" min="-2" max="2" value="0" oninput="camCtrl(this)"></label>
      <label>Quality <span id="qualityVal">24</span><input id="quality" type="range" min="10" max="30" value="24" oninput="camCtrl(this)"></label>
      <div id="log" class="log">Ready. If the page still looks plain after upload, refresh the browser cache.</div>
    </div>
  </section>
</main>
<script>
const $ = id => document.getElementById(id);
let active = false;
let poll = null;
let lastClearRevision = null;
function setLog(msg){ $('log').textContent = msg; }
function setFill(id,value){ $(id).style.width = Math.max(0, Math.min(100, Number(value)||0)) + '%'; }
async function clearIdentity(sync=true){
  $('plantName').textContent = 'Waiting for capture';
  $('plantMeta').textContent = 'No identification result yet.';
  $('confidence').textContent = '--%';
  setFill('confidenceFill', 0);
  $('identityMedia').style.display = 'none';
  $('capturePreview').removeAttribute('src');
  $('plantDetails').innerHTML = '';
  $('apiBadge').textContent = 'Hybrid ready';
  $('apiBadge').className = 'badge';
  if(sync){
    try{
      await fetch('/ctl?var=clear&val=1&t=' + Date.now(), {cache:'no-store'});
      setLog('Result cleared.');
      await getStatus();
    }catch(err){
      setLog('Clear failed. Check the ESP32 connection and try again.');
    }
  }
}
function esc(value){
  return String(value || '').replace(/[&<>"']/g, ch => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[ch]));
}
function plantDetailsHtml(d){
  const rows = [];
  if(d.healthStatus){
    rows.push('<div><b>Visual Health Check:</b> ' + esc(d.healthStatus) + ' (' + Number(d.healthConfidence || 0).toFixed(0) + '%)</div>');
    rows.push('<div><b>Health Basis:</b> Brightness ' + Number(d.brightness || 0).toFixed(0) + ', green signal ' + Number(d.green || 0).toFixed(1) + '%, plant score ' + Number(d.score || 0).toFixed(1) + '.</div>');
  }
  if(d.healthNote) rows.push('<div><b>Health Note:</b> ' + esc(d.healthNote) + '</div>');
  if(d.about) rows.push('<div><b>Description:</b> ' + esc(d.about) + '</div>');
  if(d.light) rows.push('<div><b>Light:</b> ' + esc(d.light) + '</div>');
  if(d.water) rows.push('<div><b>Water:</b> ' + esc(d.water) + '</div>');
  return rows.join('') || '<div><b>Details:</b> No plant description saved yet.</div>';
}
function startStream(sync=true){
  const img = $('stream');
  img.src = window.location.protocol + '//' + window.location.hostname + ':81/stream';
  img.style.display = 'block';
  $('empty').style.display = 'none';
  $('startBtn').style.display = 'none';
  $('stopBtn').style.display = 'inline-block';
  $('captureBtn').disabled = false;
  active = true;
  if(sync) fetch('/ctl?var=stream&val=1').catch(()=>{});
  poll = poll || setInterval(getStatus, 900);
  setLog('Stream started. Hold the plant steady, then capture.');
  getStatus();
}
function stopStream(sync=true){
  $('stream').src = '';
  $('stream').style.display = 'none';
  $('empty').style.display = 'grid';
  $('startBtn').style.display = 'inline-block';
  $('stopBtn').style.display = 'none';
  $('captureBtn').disabled = true;
  $('reticle').style.display = 'none';
  active = false;
  if(sync) fetch('/ctl?var=stream&val=0').catch(()=>{});
  setLog('Stream stopped.');
}
function camCtrl(el){
  const key = el.id === 'cam_brightness' ? 'brightness' : el.id;
  const val = el.value;
  const label = el.id === 'cam_brightness' ? $('brightVal') : $(el.id + 'Val');
  if(label) label.textContent = val;
  fetch('/ctl?var=' + key + '&val=' + val).catch(()=>{});
}
function setThreshold(v){
  $('thresholdVal').textContent = v;
  fetch('/ctl?var=threshold&val=' + v).catch(()=>{});
}
function captureErrorMessage(data){
  const text = String((data && (data.detail || data.error)) || 'Identification failed');
  if(text.startsWith('{') || text.includes('"localModel"')){
    return 'EcoDex was not confident enough, and Pl@ntNet did not return a usable match.';
  }
  return text;
}
async function capturePlant(){
  const status = await getStatus();
  if(!status || !status.stream){
    setLog('Start the stream first, then capture.');
    $('apiBadge').textContent = 'Start stream first';
    $('apiBadge').className = 'badge warn';
    return;
  }
  clearIdentity(false);
  $('captureBtn').disabled = true;
  $('apiBadge').textContent = 'Identifying...';
  $('apiBadge').className = 'badge';
  setLog('Capturing the current frame for EcoDex, then Pl@ntNet if needed.');
  try{
    const r = await fetch('/capture');
    const data = await r.json();
    if(!data.success){
      await getStatus();
      if(data.captured){
        $('apiBadge').textContent = 'Captured, no ID';
        $('apiBadge').className = 'badge warn';
        setLog(captureErrorMessage(data));
        return;
      }
      throw new Error(captureErrorMessage(data));
    }
    await getStatus();
    $('apiBadge').textContent = 'Hybrid result received';
    setLog('Identification complete.');
  }catch(err){
    await getStatus();
    $('apiBadge').textContent = 'Capture failed';
    $('apiBadge').className = 'badge warn';
    setLog(err.message || 'Could not identify this capture.');
  }finally{
    $('captureBtn').disabled = false;
  }
}
function renderReticle(d){
  const box = $('reticle');
  if(!d.detected || d.blocked || !d.tw || !d.th){
    box.style.display = 'none';
    return;
  }
  box.style.display = 'block';
  box.style.left = d.tx + '%';
  box.style.top = d.ty + '%';
  box.style.width = d.tw + '%';
  box.style.height = d.th + '%';
}
async function getStatus(){
  try{
    const r = await fetch('/status');
    const d = await r.json();
    $('detectBadge').textContent = d.detected ? 'Plant detected' : 'Searching leaves';
    $('detectBadge').className = d.detected ? 'badge' : 'badge warn';
    $('blockBadge').textContent = d.blocked ? 'View blocked' : 'View clear';
    $('blockBadge').className = d.blocked ? 'badge warn' : 'badge';
    $('score').textContent = Number(d.score || 0).toFixed(1);
    $('green').textContent = Number(d.green || 0).toFixed(1) + '%';
    $('threshold').value = d.threshold || $('threshold').value;
    $('thresholdVal').textContent = d.threshold || $('threshold').value;
    if(d.stream && !active) startStream(false);
    if(!d.stream && active) stopStream(false);
    $('captureBtn').disabled = !d.stream;
    if(lastClearRevision === null) lastClearRevision = d.clearRevision || 0;
    if((d.clearRevision || 0) !== lastClearRevision && !d.plant && !d.hasPhoto){
      lastClearRevision = d.clearRevision || 0;
      clearIdentity(false);
    }else{
      lastClearRevision = d.clearRevision || 0;
    }
    setFill('scoreFill', d.score || 0);
    setFill('greenFill', d.green || 0);
    renderReticle(d);
    if(d.plant){
      $('plantName').textContent = d.commonName || d.plant;
      $('plantMeta').textContent = d.plant + (d.family ? ' | ' + d.family : '');
      $('confidence').textContent = Number(d.confidence || 0).toFixed(0) + '%';
      setFill('confidenceFill', d.confidence || 0);
      if(d.hasPhoto){
        $('capturePreview').src = '/last-capture.jpg?t=' + Date.now();
        $('identityMedia').style.display = 'grid';
      }
      $('plantDetails').innerHTML = plantDetailsHtml(d);
      if(d.source){
        $('apiBadge').textContent =
          d.source === 'local_model' ? 'EcoDex model result' :
          'Pl@ntNet fallback result';
      }
      $('apiBadge').className = 'badge';
    }else if(d.healthStatus && d.hasPhoto){
      $('plantName').textContent = 'Unknown plant';
      $('plantMeta').textContent = 'Species was not identified, but the visual health check is available.';
      $('confidence').textContent = '--%';
      setFill('confidenceFill', 0);
      $('capturePreview').src = '/last-capture.jpg?t=' + Date.now();
      $('identityMedia').style.display = 'grid';
      $('plantDetails').innerHTML = plantDetailsHtml(d);
    }else if(!$('captureBtn').disabled && $('plantName').textContent !== 'Waiting for capture'){
      clearIdentity(false);
    }
    return d;
  }catch(err){
    if(active) setLog('Waiting for camera telemetry...');
    return null;
  }
}
poll = setInterval(getStatus, 900);
getStatus();
</script>
</body>
</html>
)rawliteral";

// Hybrid API upload

// Upload image to the hybrid backend. The backend tries EcoDex ML first,
// then falls back to Pl@ntNet when the local model is unsure.
bool identifyImageWithAPI(const uint8_t *imageData, size_t imageLen) {
  if (!imageData || imageLen == 0 || identificationInProgress) return false;

  identificationInProgress = true;
  lastApiError = "";
  lastApiStatus = 0;
  Serial.println("Sending image to EcoDex hybrid API...");

  HTTPClient http;
  String apiUrl = PLANTNET_API_URL;
  int organPos = apiUrl.indexOf("organ=");
  if (organPos >= 0) {
    int organEnd = apiUrl.indexOf("&", organPos);
    if (organEnd < 0) organEnd = apiUrl.length();
    apiUrl = apiUrl.substring(0, organPos + 6) + selectedOrgan + apiUrl.substring(organEnd);
  } else {
    apiUrl += (apiUrl.indexOf("?") >= 0) ? "&organ=" : "?organ=";
    apiUrl += selectedOrgan;
  }
  http.begin(apiUrl);
  http.addHeader("Content-Type", "image/jpeg");
  http.setTimeout(15000);

  int httpCode = http.POST((uint8_t*)imageData, imageLen);
  lastApiStatus = httpCode;

  if (httpCode == 200) {
    String response = http.getString();
    Serial.println("API response received");
    Serial.println(response);

    // Hybrid raw response:
    // {"source":"local_model|plantnet_api","scientificName":"...","commonName":"...","confidence":87}
    lastIdentifiedPlant = jsonStringValue(response, "scientificName");
    if (lastIdentifiedPlant.length() == 0) {
      lastIdentifiedPlant = jsonStringValue(response, "plant");
    }

    if (lastIdentifiedPlant.length() > 0) {
      lastSource = jsonStringValue(response, "source");
      lastCommonName = jsonStringValue(response, "commonName");
      lastFamily = jsonStringValue(response, "family");
      lastAbout = jsonStringValue(response, "about");
      lastLight = jsonStringValue(response, "light");
      lastWater = jsonStringValue(response, "water");
      lastConfidence = jsonFloatValue(response, "confidence");

      if (lastCommonName.length() == 0) {
        int commonIdx = response.indexOf("\"commonNames\"");
        if (commonIdx > 0) {
          int commonStart = response.indexOf("[", commonIdx) + 2;
          int commonEnd = response.indexOf("\"", commonStart);
          if (commonEnd > commonStart) {
            lastCommonName = response.substring(commonStart, commonEnd);
          }
        }
      }

      Serial.printf("Identified: %s (%s)\n", lastIdentifiedPlant.c_str(), lastCommonName.c_str());
      Serial.printf("   Confidence: %.0f%%\n", lastConfidence);
      Serial.printf("   Family: %s\n", lastFamily.c_str());
      Serial.printf("   Source: %s\n", lastSource.c_str());

      for (int i = 0; i < 3; i++) {
        digitalWrite(FLASH_PIN, HIGH);
        delay(100);
        digitalWrite(FLASH_PIN, LOW);
        delay(100);
      }

      http.end();
      identificationInProgress = false;
      return true;
    }

    Serial.println("Hybrid API response did not contain a plant name.");
    lastApiError = "Hybrid API response did not contain a plant name.";
  } else {
    Serial.printf("API Error: %d\n", httpCode);
    if (httpCode > 0) {
      lastApiError = http.getString();
      Serial.println(lastApiError);
    } else {
      lastApiError = http.errorToString(httpCode);
    }
  }

  http.end();
  identificationInProgress = false;
  return false;
}

bool identifyPlantWithAPI(camera_fb_t *fb) {
  if (!fb) return false;
  return identifyImageWithAPI(fb->buf, fb->len);
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
        Serial.println("Auto-capturing stable plant...");

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

// Plant detection

#define DET_W 320
#define DET_H 240

void updateDetectionFromFrame(camera_fb_t *fb) {
  if (!fb) return;
  const int detStep = 4;
  const int detW = fb->width;
  const int detH = fb->height;
  const size_t rgbLen = (size_t)detW * detH * 3;
  uint8_t *rgb = (uint8_t *)(psramFound() ? ps_malloc(rgbLen) : malloc(rgbLen));
  bool ok = rgb && fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, rgb);

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
    Serial.printf("PLANT    green=%.1f%%  box=%d,%d %dx%d\n",
                  gPct, (int)trackX, (int)trackY, (int)trackW, (int)trackH);
  }

}

void runDetection() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return;
  updateDetectionFromFrame(fb);
  esp_camera_fb_return(fb);
}

void detectionTask(void *param) {
  for (;;) {
    if (streamActive) {
      if (AUTO_CAPTURE_ENABLED) {
        autoCapturePlant();
      }
      vTaskDelay(pdMS_TO_TICKS(500));
    } else {
      runDetection();
      vTaskDelay(pdMS_TO_TICKS(1200));
    }
  }
}

// HTTP handlers

void setupOta() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  if (OTA_PASSWORD && strlen(OTA_PASSWORD) > 0) {
    ArduinoOTA.setPassword(OTA_PASSWORD);
  }

  ArduinoOTA
    .onStart([]() {
      streamActive = false;
      identificationInProgress = true;
      Serial.println("OTA update started");
    })
    .onEnd([]() {
      Serial.println("\nOTA update complete");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("OTA progress: %u%%\r", (progress * 100) / total);
    })
    .onError([](ota_error_t error) {
      identificationInProgress = false;
      Serial.printf("OTA error: %u\n", error);
    });

  ArduinoOTA.begin();
  Serial.print("OTA ready as ");
  Serial.println(OTA_HOSTNAME);
}

static esp_err_t stream_handler(httpd_req_t *req) {
  char part_buf[64];
  uint32_t lastStreamDetect = 0;
  httpd_resp_set_type(req, STREAM_CT);
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
  httpd_resp_set_hdr(req, "Pragma", "no-cache");
  httpd_resp_set_hdr(req, "X-Framerate", "15");

  while (true) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) break;

    size_t hlen = snprintf(part_buf, 64, STREAM_PT, fb->len);
    esp_err_t res = httpd_resp_send_chunk(req, part_buf, hlen);
    if (res == ESP_OK)
      res = httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len);
    if (res == ESP_OK)
      res = httpd_resp_send_chunk(req, STREAM_BND, strlen(STREAM_BND));

    if (res == ESP_OK && streamActive && !identificationInProgress &&
        (millis() - lastStreamDetect) >= STREAM_DETECT_INTERVAL_MS) {
      updateDetectionFromFrame(fb);
      if (AUTO_CAPTURE_ENABLED) {
        autoCapturePlant();
      }
      lastStreamDetect = millis();
    }

    esp_camera_fb_return(fb);
    if (res != ESP_OK) break;
    vTaskDelay(pdMS_TO_TICKS(STREAM_FRAME_DELAY_MS));
  }
  return ESP_OK;
}

static esp_err_t status_handler(httpd_req_t *req) {
  char json[2300];
  uint32_t ageMs = lastDetectMs ? (millis() - lastDetectMs) : 0;

  String safePlantName = jsonEscape(lastIdentifiedPlant);
  String safeCommonName = jsonEscape(lastCommonName);
  String safeFamily = jsonEscape(lastFamily);
  String safeSource = jsonEscape(lastSource);
  String safeAbout = jsonEscape(lastAbout);
  String safeLight = jsonEscape(lastLight);
  String safeWater = jsonEscape(lastWater);
  String safeHealthStatus = jsonEscape(lastHealthStatus);
  String safeHealthNote = jsonEscape(lastHealthNote);
  String safeOrgan = jsonEscape(selectedOrgan);

  snprintf(json, sizeof(json),
    "{\"stream\":%s,\"detected\":%s,\"green\":%.1f,\"vivid\":%.1f,"
    "\"score\":%.1f,\"blocked\":%s,\"brightness\":%.1f,\"threshold\":%d,"
    "\"age\":%lu,\"tx\":%d,\"ty\":%d,\"tw\":%d,\"th\":%d,"
    "\"plant\":\"%s\",\"commonName\":\"%s\",\"family\":\"%s\",\"source\":\"%s\",\"organ\":\"%s\","
    "\"about\":\"%s\",\"light\":\"%s\",\"water\":\"%s\","
    "\"healthStatus\":\"%s\",\"healthNote\":\"%s\",\"healthConfidence\":%d,"
    "\"hasPhoto\":%s,\"confidence\":%.0f,\"clearRevision\":%lu}",
    streamActive ? "true" : "false",
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
    safeSource.c_str(),
    safeOrgan.c_str(),
    safeAbout.c_str(),
    safeLight.c_str(),
    safeWater.c_str(),
    safeHealthStatus.c_str(),
    safeHealthNote.c_str(),
    lastHealthConfidence,
    lastCaptureJpeg ? "true" : "false",
    lastConfidence,
    (unsigned long)clearRevision);

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, json, strlen(json));
}

static esp_err_t last_capture_handler(httpd_req_t *req) {
  if (!lastCaptureJpeg || lastCaptureLen == 0) {
    httpd_resp_set_type(req, "application/json");
    const char* msg = "{\"success\":false,\"error\":\"No captured photo\"}";
    return httpd_resp_send(req, msg, strlen(msg));
  }

  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, (const char*)lastCaptureJpeg, lastCaptureLen);
}

void stopStreamState() {
  streamActive = false;
  plantDetected = false;
  viewBlocked   = false;
  lastGreenPct = lastVividPct = lastPlantScore = 0.0f;
  lastBrightness = 0.0f;
  lastDetectMs = 0;
  trackX = trackY = trackW = trackH = 0;
}

bool captureAndIdentifyFrame() {
  if (identificationInProgress) {
    lastApiError = "Identification in progress";
    return false;
  }

  if (!streamActive) {
    lastApiError = "Start stream before capture";
    return false;
  }

  clearIdentityState();
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    lastApiError = "Camera capture failed";
    return false;
  }

  updateDetectionFromFrame(fb);

  size_t imageLen = fb->len;
  uint8_t *imageCopy = (uint8_t *)(psramFound() ? ps_malloc(imageLen) : malloc(imageLen));
  if (!imageCopy) {
    esp_camera_fb_return(fb);
    lastApiError = "Not enough memory for capture";
    return false;
  }

  memcpy(imageCopy, fb->buf, imageLen);
  esp_camera_fb_return(fb);
  updateVisualHealthEstimate();
  saveLastCapture(imageCopy, imageLen);

  if (viewBlocked) {
    lastApiError = "Unknown plant: the view is blocked or unclear, so identification was skipped.";
    free(imageCopy);
    return false;
  }

  if (!plantDetected) {
    lastApiError = "Unknown plant: EcoDex did not detect a visible plant, so identification was skipped.";
    free(imageCopy);
    return false;
  }

  bool success = identifyImageWithAPI(imageCopy, imageLen);
  free(imageCopy);
  return success;
}

// Manual capture endpoint
static esp_err_t capture_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  bool success = captureAndIdentifyFrame();

  if (success) {
    const char* msg = "{\"success\":true}";
    httpd_resp_send(req, msg, strlen(msg));
  } else {
    char msg[320];
    String safeError = lastApiError;
    safeError.replace("\r", " ");
    safeError.replace("\n", " ");
    safeError.replace("\\", "\\\\");
    safeError.replace("\"", "\\\"");
    if (safeError.length() > 170) {
      safeError = safeError.substring(0, 170);
    }
    snprintf(msg, sizeof(msg),
      "{\"success\":false,\"captured\":%s,\"error\":\"Identification failed\",\"status\":%d,\"detail\":\"%s\"}",
      lastCaptureJpeg ? "true" : "false",
      lastApiStatus,
      safeError.length() ? safeError.c_str() : "No detail from hybrid API");
    httpd_resp_send(req, msg, strlen(msg));
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
    else if (!strcmp(var, "clear")) {
      clearIdentityState();
      Serial.println("Web button: identity cleared");
    }
    else if (!strcmp(var, "stream")) {
      streamActive = (v == 1);
      if (!streamActive) {
        stopStreamState();
      }
    }
  }
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, "OK", 2);
}

static esp_err_t index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
}

void handlePhysicalButtons() {
  static int lastStreamState = HIGH;
  static int lastCaptureState = HIGH;
  static int lastClearState = HIGH;
  static uint32_t lastStreamPress = 0;
  static uint32_t lastCapturePress = 0;
  static uint32_t lastClearPress = 0;
  const uint32_t debounceMs = 350;

  uint32_t now = millis();
  int streamState = digitalRead(BUTTON_STREAM_PIN);
  int captureState = digitalRead(BUTTON_CAPTURE_PIN);
  int clearState = digitalRead(BUTTON_CLEAR_PIN);

  if (clearState != lastClearState) {
    Serial.printf("Clear button level: %s\n", clearState == LOW ? "LOW" : "HIGH");
  }

  if (lastStreamState == HIGH && streamState == LOW && (now - lastStreamPress) > debounceMs) {
    lastStreamPress = now;
    if (streamActive) {
      stopStreamState();
      Serial.println("Physical button: stream stopped");
    } else {
      streamActive = true;
      Serial.println("Physical button: stream started");
    }
  }

  if (lastCaptureState == HIGH && captureState == LOW && (now - lastCapturePress) > debounceMs) {
    lastCapturePress = now;
    Serial.println("Physical button: capture requested");
    bool success = captureAndIdentifyFrame();
    Serial.println(success ? "Physical button: identification complete" : "Physical button: capture failed");
  }

  if (lastClearState == HIGH && clearState == LOW && (now - lastClearPress) > debounceMs) {
    lastClearPress = now;
    clearIdentityState();
    Serial.println("Physical button: identity cleared");
  }

  lastStreamState = streamState;
  lastCaptureState = captureState;
  lastClearState = clearState;
}

// Setup
void setup() {
  Serial.begin(115200);
  pinMode(FLASH_PIN,  OUTPUT);
  pinMode(BUTTON_STREAM_PIN, INPUT_PULLUP);
  pinMode(BUTTON_CAPTURE_PIN, INPUT_PULLUP);
  pinMode(BUTTON_CLEAR_PIN, INPUT_PULLUP);
  digitalWrite(FLASH_PIN, LOW);
  Serial.printf("Button pins: Start=GPIO%d  Capture=GPIO%d  Clear=GPIO%d\n",
                BUTTON_STREAM_PIN, BUTTON_CAPTURE_PIN, BUTTON_CLEAR_PIN);

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
  cfg.jpeg_quality = DEFAULT_JPEG_QUALITY;
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
  s->set_quality(s, DEFAULT_JPEG_QUALITY);
  s->set_vflip(s,    1);
  s->set_hmirror(s,  0);
  s->set_whitebal(s, 1);
  s->set_awb_gain(s, 1);

  // WiFi
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(400); Serial.print("."); }
  Serial.println("\nEcoDex Ready!");
  Serial.println("IP: http://" + WiFi.localIP().toString());
  Serial.printf("API: %s\n", PLANTNET_API_URL);
  setupOta();

  // HTTP server
  httpd_config_t hcfg  = HTTPD_DEFAULT_CONFIG();
  hcfg.server_port     = 80;
  hcfg.max_uri_handlers = 12;
  hcfg.stack_size      = 8192;

  httpd_config_t scfg  = hcfg;
  scfg.server_port     = 81;
  scfg.ctrl_port       = hcfg.ctrl_port + 1;

  httpd_uri_t uIndex   = {"/",        HTTP_GET, index_handler,   nullptr};
  httpd_uri_t uStatus  = {"/status",  HTTP_GET, status_handler,  nullptr};
  httpd_uri_t uCtl     = {"/ctl",     HTTP_GET, ctl_handler,     nullptr};
  httpd_uri_t uCapture = {"/capture", HTTP_GET, capture_handler, nullptr};
  httpd_uri_t uLast    = {"/last-capture.jpg", HTTP_GET, last_capture_handler, nullptr};
  httpd_uri_t uStream  = {"/stream",  HTTP_GET, stream_handler,  nullptr};

  if (httpd_start(&camera_httpd, &hcfg) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &uIndex);
    httpd_register_uri_handler(camera_httpd, &uStatus);
    httpd_register_uri_handler(camera_httpd, &uCtl);
    httpd_register_uri_handler(camera_httpd, &uCapture);
    httpd_register_uri_handler(camera_httpd, &uLast);
    Serial.println("UI server started on port 80");
  }

  if (httpd_start(&stream_httpd, &scfg) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &uStream);
    Serial.println("Stream server started on port 81");
  }

  xTaskCreatePinnedToCore(detectionTask, "detect", 16384, nullptr, 1, nullptr, 0);

  Serial.println("\nEcoDex Plant Cam with Pl@ntNet API ready!");
  Serial.println("Point camera at a plant, then press Capture & Identify");
}

void loop() {
  ArduinoOTA.handle();
  handlePhysicalButtons();
  vTaskDelay(pdMS_TO_TICKS(20));
}
