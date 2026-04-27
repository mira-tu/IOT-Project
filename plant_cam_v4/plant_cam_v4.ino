#include "esp_camera.h"
#include <WiFi.h>
#include "wifi_secrets.h"
#include "esp_http_server.h"

static esp_err_t stream_handler(httpd_req_t *req);
static esp_err_t status_handler(httpd_req_t *req);
static esp_err_t ctl_handler(httpd_req_t *req);
static esp_err_t index_handler(httpd_req_t *req);
#include "img_converters.h"   // fmt2rgb888 — always present in ESP32 Arduino core

// ─── WiFi ────────────────────────────────────────────────────────────────────

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

// ─── Detection state (shared between task and HTTP handlers) ─────────────────
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

// ─── Stream constants ────────────────────────────────────────────────────────
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* STREAM_CT  = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* STREAM_BND = "\r\n--" PART_BOUNDARY "\r\n";
static const char* STREAM_PT  = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t camera_httpd = NULL;
httpd_handle_t stream_httpd = NULL;

// ════════════════════════════════════════════════════════════════════════════
//  HTML PAGE
// ════════════════════════════════════════════════════════════════════════════
static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<!doctype html><html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>EcoDex Plant Cam</title>
<style>
:root{
  --bg:#edf2e7;
  --ink:#173629;
  --muted:#60786b;
  --panel:rgba(255,255,255,.84);
  --line:rgba(23,54,41,.12);
  --leaf:#2f7a54;
  --leaf-soft:#8bc76f;
  --gold:#c79b43;
  --alert:#b04d44;
  --shadow:0 24px 55px rgba(23,54,41,.16);
}
*{box-sizing:border-box}
body{
  min-height:100vh;
  margin:0;
  padding:12px;
  overflow:hidden;
  color:var(--ink);
  font-family:"Trebuchet MS","Segoe UI",sans-serif;
  background:
    radial-gradient(circle at top left, rgba(139,199,111,.42), transparent 28%),
    radial-gradient(circle at top right, rgba(47,122,84,.16), transparent 24%),
    linear-gradient(180deg,#f8fbf4,#edf2e7);
}
h1{
  margin:0;
  font-family:Georgia,"Times New Roman",serif;
  font-size:clamp(34px,4vw,50px);
  letter-spacing:.01em;
  line-height:1;
}
.container{
  width:min(100%,1540px);
  height:calc(100vh - 24px);
  margin:0 auto;
  display:grid;
  grid-template-columns:minmax(0,1.05fr) minmax(360px,.95fr);
  gap:16px;
  padding:16px;
  border:1px solid var(--line);
  border-radius:28px;
  background:var(--panel);
  box-shadow:var(--shadow);
  backdrop-filter:blur(12px);
}
.left-panel,.right-panel{
  min-height:0;
  display:flex;
  flex-direction:column;
}
.left-panel{gap:12px}
.right-panel{gap:12px}
.brand-row{
  display:flex;
  justify-content:space-between;
  align-items:center;
  gap:14px;
}
.status-bar{
  text-align:center;
  font-size:clamp(17px,2vw,24px);
  padding:12px 16px;
  border-radius:20px;
  transition:background .3s,color .3s,box-shadow .3s;
  font-weight:700;
}
.s-idle{background:#e7ebe4;color:#516659}
.s-search{background:linear-gradient(135deg,#244d3c,#3d7f61);color:#fff;box-shadow:0 14px 30px rgba(36,77,60,.24)}
.s-plant{background:linear-gradient(135deg,#2d684a,#58a161);color:#fff;box-shadow:0 14px 30px rgba(45,104,74,.24)}
.s-blocked{background:linear-gradient(135deg,#8c3a34,#bf5f53);color:#fff;box-shadow:0 14px 30px rgba(140,58,52,.2)}
.info{
  display:grid;
  grid-template-columns:repeat(5,minmax(0,1fr));
  gap:10px;
}
.info-card{
  min-width:0;
  padding:11px 12px;
  border-radius:18px;
  background:rgba(255,255,255,.88);
  border:1px solid var(--line);
}
.info-label{
  display:block;
  min-height:24px;
  font-size:10px;
  letter-spacing:.14em;
  text-transform:uppercase;
  color:var(--muted);
  margin-bottom:6px;
  font-weight:700;
}
.info-value{
  display:flex;
  align-items:flex-end;
  gap:5px;
  font-size:clamp(21px,2.4vw,30px);
  line-height:1;
  font-weight:700;
}
.info-value small{
  font-size:12px;
  color:var(--muted);
  padding-bottom:3px;
}
.metric-bar{
  height:7px;
  margin-top:8px;
  border-radius:999px;
  overflow:hidden;
  background:rgba(23,54,41,.08);
}
.metric-fill{
  width:0;
  height:100%;
  border-radius:999px;
  transition:width .25s ease;
  background:linear-gradient(90deg,var(--leaf),var(--leaf-soft));
}
.metric-fill.score{background:linear-gradient(90deg,var(--gold),#ecd47d)}
.btn-row{display:flex;gap:8px;flex-wrap:wrap;justify-content:flex-end}
button{
  padding:11px 18px;
  border:none;
  border-radius:999px;
  font-size:14px;
  font-weight:700;
  cursor:pointer;
  margin:0;
  transition:transform .16s ease,box-shadow .16s ease;
}
button:hover{transform:translateY(-1px)}
.btn-start{background:linear-gradient(135deg,#1d5b42,#3f9f69);color:#fff;box-shadow:0 12px 24px rgba(29,91,66,.2)}
.btn-stop{background:linear-gradient(135deg,#8d352f,#ca6354);color:#fff;box-shadow:0 12px 24px rgba(141,53,47,.18)}
.btn-capture{background:linear-gradient(135deg,#c79b43,#ecd47d);color:#173629;box-shadow:0 12px 24px rgba(199,155,67,.2)}
button:disabled{opacity:.58;cursor:not-allowed;transform:none;box-shadow:none}
button:disabled:hover{transform:none}
.cam-wrap{
  position:relative;
  flex:1 1 auto;
  min-height:260px;
  width:100%;
  background:
    linear-gradient(180deg, rgba(7,16,12,.15), rgba(7,16,12,.55)),
    radial-gradient(circle at 50% 22%, rgba(139,199,111,.28), transparent 38%),
    #0a1511;
  border-radius:24px;
  overflow:hidden;
}
#stream{
  width:100%;
  height:100%;
  display:none;
  vertical-align:top;
  object-fit:fill;
}
#box{position:absolute;display:none;border:3px solid #a8ea80;border-radius:18px;
     box-shadow:0 0 0 1px rgba(255,255,255,.18),0 0 18px rgba(168,234,128,.55);pointer-events:none;
     transition:left .25s,top .25s,width .25s,height .25s,border-color .25s,box-shadow .25s}
#box.blocked{border-color:#ff9f95;box-shadow:0 0 0 1px rgba(255,255,255,.18),0 0 18px rgba(255,159,149,.55)}
.telemetry-chip{
  position:absolute;
  left:14px;
  bottom:14px;
  padding:10px 12px;
  border-radius:999px;
  background:rgba(10,21,17,.72);
  color:#f5f8f2;
  font-size:12px;
  letter-spacing:.08em;
  text-transform:uppercase;
}
.dex-panel{
  flex:1 1 auto;
  min-height:0;
  overflow:auto;
  padding:18px;
  border-radius:24px;
  background:linear-gradient(180deg,rgba(21,42,32,.96),rgba(17,34,27,.9));
  color:#f4f7f1;
  box-shadow:0 22px 45px rgba(16,34,25,.18);
  scrollbar-width:thin;
}
.dex-header{
  display:flex;
  justify-content:space-between;
  gap:14px;
  align-items:flex-start;
}
.dex-kicker{
  display:block;
  font-size:10px;
  letter-spacing:.16em;
  text-transform:uppercase;
  color:rgba(244,247,241,.66);
  margin-bottom:7px;
  font-weight:700;
}
.dex-title{
  margin:0;
  font-size:clamp(25px,3vw,34px);
  font-family:Georgia,"Times New Roman",serif;
  line-height:1.08;
}
.dex-copy{
  margin:8px 0 0;
  color:rgba(244,247,241,.84);
  line-height:1.45;
  font-size:13px;
}
.dex-badges{
  display:flex;
  flex-wrap:wrap;
  gap:8px;
  justify-content:flex-end;
}
.dex-chip{
  display:inline-flex;
  align-items:center;
  justify-content:center;
  min-height:34px;
  padding:7px 12px;
  border-radius:999px;
  background:rgba(255,255,255,.12);
  border:1px solid rgba(255,255,255,.08);
  color:#f4f7f1;
  font-size:12px;
  font-weight:700;
}
.dex-chip.ok{background:rgba(82,163,102,.28);border-color:rgba(139,199,111,.38)}
.dex-chip.warn{background:rgba(199,155,67,.22);border-color:rgba(236,212,125,.34)}
.dex-chip.offline{background:rgba(176,77,68,.2);border-color:rgba(255,159,149,.28)}
.dex-grid{
  margin-top:12px;
  display:grid;
  grid-template-columns:repeat(2,minmax(0,1fr));
  gap:10px;
}
.dex-photo-wrap{
  display:none;
  margin-top:12px;
  height:clamp(120px,22vh,210px);
  border-radius:18px;
  overflow:hidden;
  background:rgba(255,255,255,.08);
  border:1px solid rgba(255,255,255,.08);
}
.dex-photo-wrap.has-photo{display:block}
.dex-photo-wrap img{
  display:block;
  width:100%;
  height:100%;
  object-fit:cover;
}
.dex-field{
  padding:12px 13px;
  border-radius:16px;
  background:rgba(255,255,255,.08);
  border:1px solid rgba(255,255,255,.08);
}
.dex-field span,
.dex-about span{
  display:block;
  font-size:10px;
  letter-spacing:.14em;
  text-transform:uppercase;
  color:rgba(244,247,241,.62);
  margin-bottom:7px;
  font-weight:700;
}
.dex-field strong{
  font-size:15px;
  line-height:1.35;
}
.dex-about{
  margin-top:10px;
  padding:13px;
  border-radius:16px;
  background:rgba(255,255,255,.08);
  border:1px solid rgba(255,255,255,.08);
}
.dex-about p{
  margin:0;
  color:rgba(244,247,241,.88);
  line-height:1.55;
  font-size:13px;
}
.api-row{
  margin-top:12px;
  display:grid;
  grid-template-columns:126px 1fr auto;
  gap:10px;
  align-items:center;
}
.api-row label{
  font-size:12px;
  color:rgba(244,247,241,.72);
  font-weight:700;
}
.api-row input{
  width:100%;
  min-width:0;
  border:none;
  border-radius:14px;
  padding:10px 12px;
  background:rgba(255,255,255,.12);
  color:#fff;
  font-size:13px;
}
.btn-test{
  background:linear-gradient(135deg,#c79b43,#ecd47d);
  color:#173629;
  box-shadow:none;
}
.api-help{
  margin-top:8px;
  color:rgba(244,247,241,.66);
  font-size:11px;
  line-height:1.45;
}
.controls{
  flex:0 0 auto;
  background:linear-gradient(180deg,rgba(255,255,255,.92),rgba(244,248,239,.86));
  border-radius:20px;
  padding:12px 14px;
  border:1px solid var(--line);
}
.controls summary{
  cursor:pointer;
  list-style:none;
  font-family:Georgia,"Times New Roman",serif;
  font-size:19px;
  font-weight:700;
}
.controls summary::-webkit-details-marker{display:none}
.controls summary:after{content:"Open";float:right;font-family:"Trebuchet MS","Segoe UI",sans-serif;font-size:12px;color:var(--muted);padding-top:5px}
.controls[open] summary:after{content:"Close"}
.controls[open]{max-height:36vh;overflow:auto;scrollbar-width:thin}
.ctrl-row{display:grid;grid-template-columns:118px 1fr 36px;align-items:center;gap:9px;margin:8px 0}
.ctrl-row.compact{grid-template-columns:118px auto}
.ctrl-row label{font-size:12px;color:var(--muted);font-weight:700}
.ctrl-row input[type=range]{width:100%;accent-color:var(--leaf)}
.ctrl-row select{width:100%;background:#fff;color:var(--ink);border:1px solid var(--line);padding:8px 10px;border-radius:12px}
.ctrl-row span{text-align:right;font-size:12px;font-weight:700}
.tog{position:relative;display:inline-block;width:48px;height:26px}
.tog input{opacity:0;width:0;height:0}
.knob{position:absolute;cursor:pointer;inset:0;background:rgba(23,54,41,.18);border-radius:999px;transition:.2s}
.knob:before{content:"";position:absolute;width:20px;height:20px;left:3px;bottom:3px;background:#fff;border-radius:50%;transition:.2s;box-shadow:0 5px 12px rgba(0,0,0,.18)}
input:checked+.knob{background:linear-gradient(135deg,#1d5b42,#3f9f69)}
input:checked+.knob:before{transform:translateX(22px)}
.note{
  margin-top:10px;
  padding:10px 12px;
  border-radius:16px;
  background:rgba(23,54,41,.05);
  color:var(--muted);
  font-size:12px;
  line-height:1.4;
}
@media (max-width:1100px){
  body{overflow:auto}
  .container{height:auto;min-height:calc(100vh - 24px);grid-template-columns:1fr}
  .cam-wrap{min-height:360px;aspect-ratio:4/3;flex:none}
  #stream{height:100%}
  .right-panel{min-height:auto}
  .dex-panel{overflow:visible}
}
@media (max-width:640px){
  body{padding:10px}
  .container{padding:14px;border-radius:22px}
  .brand-row{align-items:flex-start;flex-direction:column}
  .btn-row{justify-content:flex-start}
  .info{grid-template-columns:repeat(2,minmax(0,1fr))}
  .info-card:last-child{grid-column:1/-1}
  .cam-wrap{min-height:260px}
  .dex-header{flex-direction:column}
  .dex-grid{grid-template-columns:1fr}
  .api-row{grid-template-columns:1fr}
  .ctrl-row{grid-template-columns:1fr}
  .ctrl-row span{text-align:left}
  .ctrl-row.compact{grid-template-columns:1fr auto}
}
</style>
</head>
<body>
<div class="container">
  <section class="left-panel">
    <div class="brand-row">
      <h1>EcoDex</h1>
      <div class="btn-row">
        <button class="btn-start" id="btnStart" onclick="startStream()">Start Stream</button>
        <button class="btn-capture" id="btnCapture" onclick="capturePlant()" style="display:none">Capture Plant</button>
        <button class="btn-stop" id="btnStop" onclick="stopStream()" style="display:none">Stop Stream</button>
      </div>
    </div>
    <div id="statusBar" class="status-bar s-idle">Press Start to begin</div>
    <div class="info">
      <div class="info-card">
        <span class="info-label">Greenery</span>
        <div class="info-value"><b id="gPct">0.0</b><small>%</small></div>
        <div class="metric-bar"><div class="metric-fill" id="gFill"></div></div>
      </div>
      <div class="info-card">
        <span class="info-label">Vivid Greens</span>
        <div class="info-value"><b id="vividPct">0.0</b><small>%</small></div>
        <div class="metric-bar"><div class="metric-fill" id="vFill"></div></div>
      </div>
      <div class="info-card">
        <span class="info-label">Plant Score</span>
        <div class="info-value"><b id="scorePct">0.0</b><small>/100</small></div>
        <div class="metric-bar"><div class="metric-fill score" id="sFill"></div></div>
      </div>
      <div class="info-card">
        <span class="info-label">Brightness</span>
        <div class="info-value"><b id="brite">0</b><small>avg</small></div>
        <div class="metric-bar"><div class="metric-fill" id="bFill"></div></div>
      </div>
      <div class="info-card">
        <span class="info-label">Threshold</span>
        <div class="info-value"><b id="thresh">18</b><small>score</small></div>
        <div class="metric-bar"><div class="metric-fill score" id="tFill" style="width:32.7%"></div></div>
      </div>
    </div>
    <div class="cam-wrap">
      <img id="stream" src="">
      <div id="box"></div>
      <div class="telemetry-chip" id="telemetryState">Telemetry idle</div>
    </div>
  </section>
  <aside class="right-panel">
    <div class="dex-panel">
      <div class="dex-header">
        <div>
          <span class="dex-kicker">Offline EcoDex Classifier</span>
          <h3 class="dex-title" id="dexName">Waiting for a confident sighting</h3>
          <p class="dex-copy" id="dexMessage">Start the stream, then EcoDex will ask your laptop model to identify the plant.</p>
        </div>
        <div class="dex-badges">
          <span class="dex-chip" id="dexConfidence">--%</span>
          <span class="dex-chip offline" id="apiState">Laptop API not checked</span>
        </div>
      </div>
      <div class="dex-photo-wrap" id="dexPhotoWrap">
        <img id="dexPhoto" alt="Captured EcoDex plant frame">
      </div>
      <div class="dex-grid">
        <div class="dex-field">
          <span>Scientific Name</span>
          <strong id="dexScientific">-</strong>
        </div>
        <div class="dex-field">
          <span>Light Needs</span>
          <strong id="dexLight">-</strong>
        </div>
        <div class="dex-field">
          <span>Water Needs</span>
          <strong id="dexWater">-</strong>
        </div>
        <div class="dex-field">
          <span>Fun Fact</span>
          <strong id="dexFunFact">-</strong>
        </div>
      </div>
      <div class="dex-about">
        <span>About This Plant</span>
        <p id="dexAbout">Detection runs on the ESP32. Species ID runs on your offline laptop model.</p>
      </div>
      <div class="api-row">
        <label>EcoDex Laptop API</label>
        <input id="apiBase" type="text" value="http://127.0.0.1:8090" onblur="saveApiBase()">
        <button class="btn-test" onclick="testClassifier()">Test Link</button>
      </div>
      <div class="api-help">Use <code>http://127.0.0.1:8090</code> when this page is open on the same laptop. Use your laptop IP instead when opening EcoDex from another device.</div>
    </div>
    <details class="controls">
      <summary>Plant Telemetry Settings</summary>
      <div class="ctrl-row">
        <label>Score Threshold</label>
        <input type="range" id="threshold" min="6" max="55" value="18" oninput="setThreshold(this.value)">
        <span id="threshVal">18</span>
      </div>
      <div class="ctrl-row">
        <label>Resolution</label>
        <select id="framesize" onchange="camCtrl(this)">
          <option value="6">VGA 640x480</option>
          <option value="5">CIF 400x296</option>
          <option value="4" selected>QVGA 320x240</option>
        </select>
        <span>live</span>
      </div>
      <div class="ctrl-row">
        <label>JPEG Quality</label>
        <input type="range" id="quality" min="10" max="63" value="18" oninput="camCtrl(this)">
        <span id="qualityVal">18</span>
      </div>
      <div class="ctrl-row">
        <label>Brightness</label>
        <input type="range" id="cam_brightness" min="-2" max="2" value="0" oninput="camCtrl(this)">
        <span id="cam_brightnessVal">0</span>
      </div>
      <div class="ctrl-row">
        <label>Contrast</label>
        <input type="range" id="contrast" min="-2" max="2" value="0" oninput="camCtrl(this)">
        <span id="contrastVal">0</span>
      </div>
      <div class="ctrl-row">
        <label>Saturation</label>
        <input type="range" id="saturation" min="-2" max="2" value="0" oninput="camCtrl(this)">
        <span id="saturationVal">0</span>
      </div>
      <div class="ctrl-row compact">
        <label>V-Flip</label>
        <label class="tog"><input type="checkbox" id="vflip" checked onchange="camCtrl(this)"><span class="knob"></span></label>
      </div>
      <div class="ctrl-row compact">
        <label>H-Mirror</label>
        <label class="tog"><input type="checkbox" id="hmirror" checked onchange="camCtrl(this)"><span class="knob"></span></label>
      </div>
      <div class="ctrl-row compact">
        <label>AWB</label>
        <label class="tog"><input type="checkbox" id="awb" checked onchange="camCtrl(this)"><span class="knob"></span></label>
      </div>
      <div class="note">Telemetry still runs fully on the ESP32. EcoDex species ID comes from your offline laptop classifier when the view is clear enough to capture.</div>
    </details>
  </aside>
</div>
<canvas id="snapshotCanvas" style="display:none"></canvas>
<script>
let active = false, poll = null;
let classifyTimer = null;
let classifyBusy = false;
let classifierHealthy = false;
let lastApiCheckMs = 0;
let lastClassifyMs = 0;
let lastStatus = { detected:false, blocked:false, age:0 };
let hasLockedDexResult = false;
let lastDexPhotoUrl = '';
let lastDexResult = null;
const DEFAULT_API_BASE = 'http://127.0.0.1:8090';
const CLASSIFY_INTERVAL_MS = 3200;
const API_HEALTH_WINDOW_MS = 15000;
const $ = id => document.getElementById(id);

var setBar = function(id, value){
  $(id).style.width = Math.max(0, Math.min(100, value)) + '%';
}

var normalizeApiBase = function(value){
  let v = (value || '').trim();
  if(!v) v = DEFAULT_API_BASE;
  if(v.indexOf('http://') !== 0 && v.indexOf('https://') !== 0) v = 'http://' + v;
  while(v.length > 0 && v.charAt(v.length - 1) === '/') v = v.substring(0, v.length - 1);
  return v;
}

var saveApiBase = function(){
  const normalized = normalizeApiBase($('apiBase').value);
  const changed = $('apiBase').dataset.saved !== normalized;
  $('apiBase').value = normalized;
  $('apiBase').dataset.saved = normalized;
  try { localStorage.setItem('ecodexApiBase', normalized); } catch (e) {}
  if(changed) classifierHealthy = false;
  return normalized;
}

var setApiState = function(text, ok){
  $('apiState').textContent = text;
  $('apiState').className = 'dex-chip ' + (ok ? 'ok' : 'offline');
}

var clearDexPhoto = function(){
  if(lastDexPhotoUrl){
    URL.revokeObjectURL(lastDexPhotoUrl);
    lastDexPhotoUrl = '';
  }
  $('dexPhoto').removeAttribute('src');
  $('dexPhotoWrap').className = 'dex-photo-wrap';
}

var showDexPhoto = function(blob){
  if(!blob) return;
  clearDexPhoto();
  lastDexPhotoUrl = URL.createObjectURL(blob);
  $('dexPhoto').src = lastDexPhotoUrl;
  $('dexPhotoWrap').className = 'dex-photo-wrap has-photo';
}

var resetDexCard = function(){
  hasLockedDexResult = false;
  lastDexResult = null;
  clearDexPhoto();
  $('dexName').textContent = 'Waiting for a confident sighting';
  $('dexMessage').textContent = 'Start the stream, then EcoDex will ask your laptop model to identify the plant.';
  $('dexConfidence').textContent = '--%';
  $('dexConfidence').className = 'dex-chip';
  $('dexScientific').textContent = '—';
  $('dexLight').textContent = '—';
  $('dexWater').textContent = '—';
  $('dexFunFact').textContent = '—';
  $('dexAbout').textContent = 'Detection runs on the ESP32. Species ID runs on your offline laptop model.';
}

var setDexPending = function(message){
  if(hasLockedDexResult) return;
  $('dexName').textContent = 'EcoDex is watching';
  $('dexMessage').textContent = message;
  $('dexConfidence').textContent = '--%';
  $('dexConfidence').className = 'dex-chip';
  $('dexScientific').textContent = '—';
  $('dexLight').textContent = '—';
  $('dexWater').textContent = '—';
  $('dexFunFact').textContent = '—';
  $('dexAbout').textContent = 'When the view stays clear, EcoDex captures a frame and compares it to the plants trained on your laptop.';
}

var setDexUnknown = function(message){
  if(hasLockedDexResult) return;
  $('dexName').textContent = 'EcoDex not sure yet';
  $('dexMessage').textContent = message;
  $('dexConfidence').textContent = '--%';
  $('dexConfidence').className = 'dex-chip warn';
  $('dexScientific').textContent = '—';
  $('dexLight').textContent = 'Unknown';
  $('dexWater').textContent = 'Unknown';
  $('dexFunFact').textContent = 'More labeled photos make EcoDex more confident.';
  $('dexAbout').textContent = 'This frame did not strongly match one trained plant class.';
}

var renderDexResult = function(result, photoBlob){
  const facts = result.facts || {};
  const shouldLock = result.accepted && result.label !== 'unknown';
  if(!shouldLock && hasLockedDexResult) return;
  hasLockedDexResult = shouldLock;
  lastDexResult = shouldLock ? result : null;
  $('dexName').textContent = shouldLock ? (result.display_name || 'Plant identified') : 'EcoDex not sure yet';
  $('dexMessage').textContent = result.message || 'Classification finished.';
  $('dexConfidence').textContent = Math.round((result.confidence || 0) * 100) + '%';
  $('dexConfidence').className = 'dex-chip ' + (shouldLock ? 'ok' : 'warn');
  $('dexScientific').textContent = facts.scientific_name || '—';
  $('dexLight').textContent = facts.light || '—';
  $('dexWater').textContent = facts.water || '—';
  $('dexFunFact').textContent = facts.fun_fact || '—';
  $('dexAbout').textContent = facts.about || 'EcoDex has no card details for this plant yet.';
  if(shouldLock){
    showDexPhoto(photoBlob);
  } else {
    clearDexPhoto();
  }
}

var ensureClassifierReady = async (force) => {
  const now = Date.now();
  if(!force && classifierHealthy && (now - lastApiCheckMs) < API_HEALTH_WINDOW_MS) return true;
  lastApiCheckMs = now;
  const api = saveApiBase();
  setApiState('Checking laptop API...', false);
  try{
    const r = await fetch(api + '/health');
    if(!r.ok) throw new Error('health check failed');
    classifierHealthy = true;
    setApiState('Laptop API ready', true);
    return true;
  } catch (e) {
    classifierHealthy = false;
    setApiState('Laptop API offline', false);
    return false;
  }
}

var testClassifier = async () => {
  const ready = await ensureClassifierReady(true);
  if(ready){
    $('dexMessage').textContent = 'Laptop classifier is online. Aim at one trained plant, then press Capture Plant.';
  } else {
    $('dexMessage').textContent = 'Start .\\ecodex_local\\run_server.ps1 on your laptop, then test the link again.';
  }
}

var startStream = function(){
  const stream = $('stream');
  stream.crossOrigin = 'anonymous';
  stream.src = window.location.protocol + '//' + window.location.hostname + ':81/stream';
  $('stream').style.display = 'block';
  $('btnStart').style.display = 'none';
  $('btnStop').style.display  = 'inline-block';
  $('btnCapture').style.display = 'inline-block';
  $('statusBar').className = 'status-bar s-search';
  $('statusBar').textContent = 'Starting live scan';
  $('telemetryState').textContent = 'Telemetry warming up';
  active = true;
  fetch('/ctl?var=stream&val=1').catch(() => {});
  poll = poll || setInterval(getStatus, 800);
  setDexPending('Telemetry is warming up. Center the plant, then press Capture Plant.');
  ensureClassifierReady(false);
  getStatus();
}

var stopStream = function(){
  $('stream').src = '';
  $('stream').style.display = 'none';
  $('btnStart').style.display = 'inline-block';
  $('btnStop').style.display  = 'none';
  $('btnCapture').style.display = 'none';
  $('box').style.display = 'none';
  $('statusBar').className = 'status-bar s-idle';
  document.getElementById('statusBar').textContent = '⏸ Stream stopped';
  $('statusBar').textContent = 'Scanner idle';
  $('telemetryState').textContent = 'Telemetry idle';
  active = false;
  lastStatus = { detected:false, blocked:false, age:0 };
  classifyBusy = false;
  lastClassifyMs = 0;
  fetch('/ctl?var=stream&val=0').catch(() => {});
  clearInterval(poll); poll = null;

  resetDexCard();
}

var camCtrl = function(el){
  let v = el.type === 'checkbox' ? (el.checked ? 1 : 0) : el.value;
  let k = el.id === 'cam_brightness' ? 'brightness' : el.id;
  fetch('/ctl?var=' + k + '&val=' + v).catch(() => {});
  let sp = document.getElementById(el.id + 'Val');
  if(sp) sp.textContent = v;
}

var setThreshold = function(v){
  $('threshVal').textContent = v;
  $('thresh').textContent    = v;
  setBar('tFill', (v / 55) * 100);
  fetch('/ctl?var=threshold&val=' + v).catch(() => {});
}

var captureFrameBlob = function(){
  return new Promise((resolve, reject) => {
    const img = $('stream');
    if(!img.complete || !img.naturalWidth || !img.naturalHeight){
      reject(new Error('Camera frame not ready yet'));
      return;
    }

    const canvas = $('snapshotCanvas');
    canvas.width = img.naturalWidth;
    canvas.height = img.naturalHeight;
    const ctx = canvas.getContext('2d');

    try {
      ctx.drawImage(img, 0, 0, canvas.width, canvas.height);
    } catch (err) {
      reject(err);
      return;
    }

    canvas.toBlob(blob => {
      if(blob) resolve(blob);
      else reject(new Error('Could not capture frame'));
    }, 'image/jpeg', 0.88);
  });
}

var capturePlant = function(){
  classifyFrame(true, true);
}

var classifyFrame = async (force, manual) => {
  if(!active){
    if(manual) setDexUnknown('Start the stream first, then press Capture Plant.');
    return;
  }
  if(classifyBusy) return;
  if(!lastStatus.detected || lastStatus.blocked || lastStatus.age > 2500){
    if(manual){
      const msg = lastStatus.blocked
        ? 'View is blocked. Move the camera back so EcoDex can see leaf detail, then capture again.'
        : 'EcoDex needs a clear plant-like view first. Center the plant, hold still, then press Capture Plant.';
      setDexUnknown(msg);
    }
    return;
  }

  const now = Date.now();
  if(!force && (now - lastClassifyMs) < CLASSIFY_INTERVAL_MS) return;

  const ready = await ensureClassifierReady(false);
  if(!ready){
    lastClassifyMs = now;
    setDexUnknown('Laptop API is offline. Start .\\ecodex_local\\run_server.ps1 to enable species ID.');
    return;
  }

  classifyBusy = true;
  let captureBtn = $('btnCapture');
  if(captureBtn){
    captureBtn.disabled = true;
    captureBtn.textContent = 'Capturing...';
  }
  lastClassifyMs = now;
  $('dexMessage').textContent = 'EcoDex captured this frame and is comparing it to your offline plant model...';

  try{
    const blob = await captureFrameBlob();
    const r = await fetch(saveApiBase() + '/predict', {
      method: 'POST',
      body: blob
    });
    if(!r.ok) throw new Error('classification request failed');
    const result = await r.json();
    renderDexResult(result, blob);
  } catch (e) {
    classifierHealthy = false;
    setApiState('Laptop API offline', false);
    setDexUnknown('EcoDex could not classify this frame yet. Reload the page if the camera stream was just started.');
  } finally {
    classifyBusy = false;
    if(captureBtn){
      captureBtn.disabled = false;
      captureBtn.textContent = 'Capture Plant';
    }
  }
}

var getStatus = function(){
  if(!active) return;
  fetch('/status')
    .then(r => r.json())
    .then(d => {
      const wasDetected = lastStatus.detected;
      lastStatus = d;
      $('gPct').textContent = d.green.toFixed(1);
      $('vividPct').textContent = d.vivid.toFixed(1);
      $('scorePct').textContent = d.score.toFixed(1);
      $('brite').textContent = d.brightness.toFixed(0);
      $('thresh').textContent = d.threshold;
      $('threshVal').textContent = d.threshold;

      setBar('gFill', d.green);
      setBar('vFill', d.vivid);
      setBar('sFill', d.score);
      setBar('bFill', Math.min(100, d.brightness / 2.55));
      setBar('tFill', (d.threshold / 55) * 100);

      const bar = $('statusBar');
      const box = $('box');
      $('telemetryState').textContent = d.age < 1800 ? 'Telemetry live' : 'Telemetry stale';

      if(d.blocked){
        bar.className   = 'status-bar s-blocked';
        bar.textContent = '🚫 View Blocked!';
        box.className   = 'blocked';
        bar.textContent = 'View blocked';
        box.style.display = 'block';
        box.style.left = '2%'; box.style.top    = '2%';
        box.style.width= '96%';box.style.height = '96%';
        lastClassifyMs = 0;
        if(!classifyBusy) setDexPending('View blocked. Move the camera back so EcoDex can see leaf detail clearly.');
      } else if(d.detected){
        bar.className   = 'status-bar s-plant';
        bar.textContent = '🌿 Plant Detected!';
        box.className   = '';
        bar.textContent = 'Plant-like scene detected';
        if(d.tw > 0 && d.th > 0){
          box.style.display = 'block';
          box.style.left   = d.tx + '%';
          box.style.top    = d.ty + '%';
          box.style.width  = d.tw + '%';
          box.style.height = d.th + '%';
        } else {
          box.style.display = 'none';
        }
        if(!wasDetected && !classifyBusy) setDexPending('Plant-like scene found. Hold still, then press Capture Plant to identify it.');
      } else {
        bar.className   = 'status-bar s-search';
        bar.textContent = '🔍 Searching for plants...';
        bar.textContent = 'Scanning greenery and vivid greens';
        box.style.display = 'none';
        lastClassifyMs = 0;
        if(!classifyBusy) setDexPending('No plant-like scene yet. Point the camera at one trained plant, then press Capture Plant.');
      }
    })
    .catch(() => {
      $('telemetryState').textContent = 'Telemetry lost';
      $('statusBar').className = 'status-bar s-idle';
      $('statusBar').textContent = 'Waiting for telemetry';
      lastStatus = { detected:false, blocked:false, age:0 };
    });
}

try {
  const storedApi = localStorage.getItem('ecodexApiBase');
  if(storedApi) $('apiBase').value = storedApi;
} catch (e) {}
saveApiBase();
resetDexCard();
</script>
</body></html>
)rawliteral";


// ════════════════════════════════════════════════════════════════════════════
//  DETECTION
//  Uses fmt2rgb888() from img_converters.h to decode JPEG → raw RGB pixels.
//  Camera stays in PIXFORMAT_JPEG the whole time — no format switching.
//  Detection runs on Core 0 so HTTP (Core 1) never blocks.
// ════════════════════════════════════════════════════════════════════════════

// Frame size used for detection — must match what camera is capturing
#define DET_W 320
#define DET_H 240

void runDetection() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return;

  // Decode JPEG → RGB888 into a heap-allocated buffer
  const int detStep = 4;
  const int detW = fb->width;
  const int detH = fb->height;
  const size_t rgbLen = (size_t)detW * detH * 3;
  uint8_t *rgb = (uint8_t *)(psramFound() ? ps_malloc(rgbLen) : malloc(rgbLen));
  bool ok = rgb && fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, rgb);
  esp_camera_fb_return(fb);   // release framebuffer immediately

  if (!ok || !rgb) {
    if (rgb) free(rgb);
    return;
  }

  // ── Scan pixels ──────────────────────────────────────────────────────────
  long greenPx = 0, vividPx = 0, totalPx = 0, darkPx = 0, blockedPx = 0, sumBright = 0;
  int  minX = detW, maxX = 0, minY = detH, maxY = 0;

  // Sample every 4th pixel in X and every 4th row in Y for speed
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

      // Dark — camera covered in dim/dark conditions
      if (bright < 45) darkPx++;

      // Blocked — washed-out object very close to lens (hand, cover, etc.)
      // Must be bright AND desaturated (grey/skin-tone, not a colourful plant)
      if (bright > 175 && sat < 22) blockedPx++;

      // ── Green plant ───────────────────────────────────────────────────
      // Green clearly dominates red and blue, has colour (not grey/white)
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

  free(rgb);   // always free — never leak

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

  // Blocked: mostly dark (covered) OR mostly washed-out (hand pressed close)
  viewBlocked = (darkPct > 65.0f) || (blockPct > 70.0f);
  if (viewBlocked) plantScore = 0.0f;
  lastPlantScore = plantScore;

  // ── Tracking box (as % of frame) ─────────────────────────────────────────
  if (greenPx > 8 && maxX > minX && maxY > minY) {
    int bx = max(0,       (minX * 100 / detW) - 4);
    int by = max(0,       (minY * 100 / detH) - 4);
    int bw = min(100 - bx, ((maxX - minX) * 100 / detW) + 8);
    int bh = min(100 - by, ((maxY - minY) * 100 / detH) + 8);

    // Exponential moving average — smooth follow
    trackX = (trackX * 2 + bx) / 3;
    trackY = (trackY * 2 + by) / 3;
    trackW = (trackW * 2 + bw) / 3;
    trackH = (trackH * 2 + bh) / 3;
  } else {
    trackX = trackY = trackW = trackH = 0;
  }

  // ── State transitions + serial log ───────────────────────────────────────
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
  } else if (!plantDetected && !viewBlocked) {
    static uint32_t lastSearchLog = 0;
    uint32_t now = millis();
    if (now - lastSearchLog >= 3000) {
      lastSearchLog = now;
      Serial.printf("SEARCH   green=%.1f%%  bright=%.0f\n", gPct, avgBright);
    }
  }

  // Buzzer on while plant detected and view is clear
  if (plantDetected && !viewBlocked) {
    ledcAttach(BUZZER_PIN, 2000, 8);
    ledcWrite(BUZZER_PIN, 100);
  } else {
    ledcWrite(BUZZER_PIN, 0);
  }
}

// Detection task — pinned to Core 0 so HTTP server on Core 1 never blocks
void detectionTask(void *param) {
  for (;;) {
    if (streamActive) {
      runDetection();
      vTaskDelay(pdMS_TO_TICKS(1000));  // lighter telemetry keeps the stream smoother
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
  char json[320];
  uint32_t ageMs = lastDetectMs ? (millis() - lastDetectMs) : 0;
  snprintf(json, sizeof(json),
    "{\"detected\":%s,\"green\":%.1f,\"vivid\":%.1f,"
    "\"score\":%.1f,\"blocked\":%s,\"brightness\":%.1f,\"threshold\":%d,"
    "\"age\":%lu,\"tx\":%d,\"ty\":%d,\"tw\":%d,\"th\":%d}",
    plantDetected ? "true"  : "false",
    (float)lastGreenPct,
    (float)lastVividPct,
    (float)lastPlantScore,
    viewBlocked   ? "true"  : "false",
    (float)lastBrightness,
    plantThreshold,
    (unsigned long)ageMs,
    (int)trackX, (int)trackY, (int)trackW, (int)trackH);

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, json, strlen(json));
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
        // Clean up when stream stops
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
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
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
  cfg.pixel_format = PIXFORMAT_JPEG;   // stays JPEG always — no runtime switching
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
  s->set_framesize(s, FRAMESIZE_QVGA);   // 320x240 - optimal for smoother streaming
  s->set_quality(s, 18);             // more compression reduces WiFi lag
  s->set_vflip(s,    1);
  s->set_hmirror(s,  1);
  s->set_whitebal(s, 1);
  s->set_awb_gain(s, 1);

  // WiFi
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(400); Serial.print("."); }
  Serial.println("\nReady: http://" + WiFi.localIP().toString());

  // HTTP server
  httpd_config_t hcfg  = HTTPD_DEFAULT_CONFIG();
  hcfg.server_port     = 80;
  hcfg.max_uri_handlers = 8;
  hcfg.stack_size      = 8192;

  httpd_config_t scfg  = hcfg;
  scfg.server_port     = 81;
  scfg.ctrl_port       = hcfg.ctrl_port + 1;

  httpd_uri_t uIndex  = {"/",       HTTP_GET, index_handler,  nullptr};
  httpd_uri_t uStatus = {"/status", HTTP_GET, status_handler, nullptr};
  httpd_uri_t uCtl    = {"/ctl",    HTTP_GET, ctl_handler,    nullptr};
  httpd_uri_t uStream = {"/stream", HTTP_GET, stream_handler, nullptr};

  if (httpd_start(&camera_httpd, &hcfg) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &uIndex);
    httpd_register_uri_handler(camera_httpd, &uStatus);
    httpd_register_uri_handler(camera_httpd, &uCtl);
    Serial.println("UI server started on port 80");
  }

  if (httpd_start(&stream_httpd, &scfg) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &uStream);
    Serial.println("Stream server started on port 81");
  }

  // Detection task on Core 0 — HTTP runs on Core 1 — they never block each other
  xTaskCreatePinnedToCore(detectionTask, "detect", 16384, nullptr, 1, nullptr, 0);
}


// ════════════════════════════════════════════════════════════════════════════
//  LOOP — idle, tasks do all the work
// ════════════════════════════════════════════════════════════════════════════
void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
