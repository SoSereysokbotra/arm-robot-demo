#include "WebServer.h"
#include "Servos.h"
#include "Config.h"

void handleWebRequest(WiFiClient& client, String request) {
  if (request.indexOf("GET /base") >= 0) {
    String action = getStringParameter(request, "action");
    int speed = getParameter(request, "speed");

    if (speed < 0) speed = 60;
    speed = constrain(speed, 0, 100);

    int offset = map(speed, 0, 100, 0, 90);

    if (action == "cw") {
      setBaseSpeed(90 + offset);
      sendResponse(client, "OK: base rotating CW at " + String(speed) + "%");
    } else if (action == "ccw") {
      setBaseSpeed(90 - offset);
      sendResponse(client, "OK: base rotating CCW at " + String(speed) + "%");
    } else if (action == "stop") {
      setBaseSpeed(BASE_STOP);
      sendResponse(client, "OK: base stopped");
    } else {
      sendResponse(client, "ERROR: action must be cw, ccw, or stop.");
    }
  } 
  else if (request.indexOf("GET /servo") >= 0) {
    int ch = getParameter(request, "ch");
    int angle = getParameter(request, "angle");

    if (ch == BASE_SERVO) {
      sendResponse(client, "ERROR: base is continuous-rotation, use /base instead.");
    } else if (ch > 0 && ch < TOTAL_SERVOS && angle >= 0 && angle <= 180) {
      targetAngle[ch] = angle;
      sendResponse(client, "OK: Servo CH" + String(ch) + " moving to " + String(angle) + " degrees.");
    } else {
      sendResponse(client, "ERROR: Invalid servo command.");
    }
  } 
  else if (request.indexOf("GET /home") >= 0) {
    for (int ch = 1; ch < TOTAL_SERVOS; ch++) {
      targetAngle[ch] = 90;
    }
    setBaseSpeed(BASE_STOP);
    sendResponse(client, "Homing all joints and stopping base.");
  } 
  else if (request.indexOf("GET /") >= 0) {
    sendHomePage(client);
  }
}

int getParameter(String request, String parameter) {
  String search = parameter + "=";
  int start = request.indexOf(search);
  if (start == -1) return -1;
  start += search.length();
  int end = request.indexOf("&", start);
  if (end == -1) end = request.indexOf(" ", start);
  return request.substring(start, end).toInt();
}

String getStringParameter(String request, String parameter) {
  String search = parameter + "=";
  int start = request.indexOf(search);
  if (start == -1) return "";
  start += search.length();
  int end = request.indexOf("&", start);
  if (end == -1) end = request.indexOf(" ", start);
  return request.substring(start, end);
}

void sendResponse(WiFiClient& client, String message) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/plain");
  client.println("Connection: close");
  client.println();
  client.println(message);
}

void sendHomePage(WiFiClient& client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();

  client.print(R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>Robot Arm Pro Control</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
  :root {
    --bg: #090d12;
    --panel: #131922;
    --panel-border: #222d3d;
    --text: #f0f4f8;
    --text-dim: #8a99ad;
    --accent: #00e5bc;
    --accent-dim: #07382d;
    --danger: #ff3b30;
    --danger-dim: #3d0f0d;
    --warn: #ff9500;
    --info: #0a84ff;
  }
  * { box-sizing: border-box; -webkit-tap-highlight-color: transparent; }
  body {
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
    background: var(--bg);
    color: var(--text);
    margin: 0;
    padding: 16px;
    max-width: 520px;
    margin-inline: auto;
  }
  header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    margin-bottom: 16px;
    padding-bottom: 12px;
    border-bottom: 1px solid var(--panel-border);
  }
  h1 {
    font-size: 16px;
    font-weight: 700;
    letter-spacing: 0.05em;
    text-transform: uppercase;
    margin: 0;
    color: var(--text);
  }
  .status-badge {
    display: inline-flex;
    align-items: center;
    gap: 6px;
    font-size: 11px;
    font-weight: 600;
    background: var(--accent-dim);
    color: var(--accent);
    padding: 4px 10px;
    border-radius: 20px;
    border: 1px solid rgba(0,229,188,0.2);
  }
  .dot {
    width: 6px; height: 6px; border-radius: 50%;
    background: var(--accent);
    box-shadow: 0 0 8px var(--accent);
  }

  .panel {
    background: var(--panel);
    border: 1px solid var(--panel-border);
    border-radius: 12px;
    padding: 14px 16px;
    margin-bottom: 12px;
  }
  .panel-label {
    font-size: 10px;
    font-weight: 700;
    letter-spacing: 0.08em;
    text-transform: uppercase;
    color: var(--text-dim);
    margin-bottom: 12px;
    display: flex;
    justify-content: space-between;
  }

  /* Joints */
  .joint { margin-bottom: 14px; }
  .joint:last-child { margin-bottom: 0; }
  .joint-row {
    display: flex;
    justify-content: space-between;
    align-items: baseline;
    margin-bottom: 6px;
    font-size: 13px;
    font-weight: 600;
  }
  .joint-row span {
    font-family: monospace;
    color: var(--accent);
    background: var(--accent-dim);
    padding: 2px 6px;
    border-radius: 4px;
    font-size: 12px;
  }
  input[type=range] {
    width: 100%;
    height: 24px;
    -webkit-appearance: none;
    background: transparent;
  }
  input[type=range]::-webkit-slider-runnable-track {
    height: 6px;
    border-radius: 3px;
    background: #1c2635;
  }
  input[type=range]::-webkit-slider-thumb {
    -webkit-appearance: none;
    width: 20px; height: 20px;
    border-radius: 50%;
    background: var(--accent);
    border: 3px solid var(--panel);
    margin-top: -7px;
    box-shadow: 0 0 6px rgba(0,0,0,0.5);
  }

  /* Base Controls */
  .base-grid {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 10px;
    margin-bottom: 10px;
  }
  .base-btn {
    padding: 14px 0;
    background: #1a2330;
    border: 1px solid var(--panel-border);
    border-radius: 8px;
    color: var(--text);
    font-size: 18px;
    text-align: center;
    user-select: none;
    touch-action: none;
    font-weight: bold;
    transition: 0.1s;
  }
  .base-btn:active, .base-btn.active {
    background: var(--accent-dim);
    border-color: var(--accent);
    color: var(--accent);
  }
  .base-btn .lbl {
    display: block;
    font-size: 9px;
    letter-spacing: 0.08em;
    text-transform: uppercase;
    color: var(--text-dim);
    margin-top: 2px;
  }
  .base-status {
    text-align: center;
    font-size: 11px;
    font-weight: 700;
    text-transform: uppercase;
    color: var(--text-dim);
    padding: 6px;
    background: #0f141c;
    border-radius: 6px;
  }
  .base-status.spinning { color: var(--warn); background: rgba(255,149,0,0.1); }

  /* Presets Grid */
  .presets-grid {
    display: grid;
    grid-template-columns: repeat(3, 1fr);
    gap: 8px;
  }
  .preset-btn {
    padding: 10px 0;
    background: #1a2330;
    border: 1px solid var(--panel-border);
    border-radius: 8px;
    color: var(--text);
    font-size: 12px;
    font-weight: 600;
    cursor: pointer;
    text-align: center;
  }
  .preset-btn:active { background: #26354a; }

  /* Emergency Stop / Actions */
  .action-grid {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 10px;
  }
  .estop-btn {
    width: 100%;
    padding: 14px;
    background: var(--danger-dim);
    color: var(--danger);
    border: 1px solid rgba(255,59,48,0.3);
    border-radius: 8px;
    font-size: 13px;
    font-weight: 700;
    text-transform: uppercase;
    cursor: pointer;
  }
  .estop-btn:active { opacity: 0.8; }
  
  .home-btn {
    width: 100%;
    padding: 14px;
    background: #1a2330;
    color: var(--info);
    border: 1px solid rgba(10,132,255,0.3);
    border-radius: 8px;
    font-size: 13px;
    font-weight: 700;
    text-transform: uppercase;
    cursor: pointer;
  }
  .home-btn:active { background: #223046; }

  #logger {
    text-align: center;
    margin-top: 10px;
    font-size: 11px;
    color: var(--text-dim);
    font-family: monospace;
    min-height: 16px;
  }
</style>
</head>
<body>

<header>
  <h1>Robot Arm <span style="font-weight:300; color:var(--text-dim); font-size:12px;">v2.0</span></h1>
  <div class="status-badge"><span class="dot"></span>Connected</div>
</header>

<!-- Base Panel -->
<div class="panel">
  <div class="panel-label"><span>Base Rotation (360&deg;)</span></div>
  <div class="base-grid">
    <div class="base-btn" id="btnCcw">&#8630;<span class="lbl">Rotate CCW</span></div>
    <div class="base-btn" id="btnCw">&#8631;<span class="lbl">Rotate CW</span></div>
  </div>
  <div class="base-status" id="baseIndicator">Base Stopped</div>
  <div class="joint" style="margin-top: 12px;">
    <div class="joint-row">Rotation Speed <span id="speedVal">60</span>%</div>
    <input type="range" min="10" max="100" value="60" id="speedSlider" oninput="document.getElementById('speedVal').innerText=this.value">
  </div>
</div>

<!-- Joint Angles Panel -->
<div class="panel">
  <div class="panel-label"><span>Articulated Joints</span><span>0&deg; - 180&deg;</span></div>
  
  <div class="joint"><div class="joint-row">Shoulder <span id="val1">90</span>&deg;</div>
    <input type="range" min="0" max="180" value="90" id="slider1" oninput="onDrag(1)" onchange="onRelease(1)"></div>
  <div class="joint"><div class="joint-row">Elbow <span id="val2">90</span>&deg;</div>
    <input type="range" min="0" max="180" value="90" id="slider2" oninput="onDrag(2)" onchange="onRelease(2)"></div>
  <div class="joint"><div class="joint-row">Wrist <span id="val3">90</span>&deg;</div>
    <input type="range" min="0" max="180" value="90" id="slider3" oninput="onDrag(3)" onchange="onRelease(3)"></div>
  <div class="joint"><div class="joint-row">Wrist Rotate <span id="val4">90</span>&deg;</div>
    <input type="range" min="0" max="180" value="90" id="slider4" oninput="onDrag(4)" onchange="onRelease(4)"></div>
  <div class="joint"><div class="joint-row">Gripper <span id="val5">90</span>&deg;</div>
    <input type="range" min="0" max="180" value="90" id="slider5" oninput="onDrag(5)" onchange="onRelease(5)"></div>
</div>

<!-- Presets Panel -->
<div class="panel">
  <div class="panel-label">Quick Presets</div>
  <div class="presets-grid">
    <div class="preset-btn" onclick="setPreset(90,90,90,90,90)">Center</div>
    <div class="preset-btn" onclick="setPreset(45,135,90,90,30)">Pickup</div>
    <div class="preset-btn" onclick="setPreset(90,45,45,90,120)">Tuck</div>
  </div>
</div>

<!-- Action Controls -->
<div class="action-grid">
  <button class="home-btn" onclick="home()">Home All</button>
  <button class="estop-btn" onclick="emergencyStop()">E-Stop</button>
</div>

<div id="logger">System Ready</div>

<script>
var lastSent = {};

function onDrag(ch) {
  document.getElementById('val' + ch).innerText = document.getElementById('slider' + ch).value;
  var now = Date.now();
  if (!lastSent[ch] || now - lastSent[ch] > 90) { 
    lastSent[ch] = now; 
    sendServo(ch); 
  }
}
function onRelease(ch) { sendServo(ch); }

function sendServo(ch) {
  var angle = document.getElementById('slider' + ch).value;
  fetch('/servo?ch=' + ch + '&angle=' + angle)
    .then(r => r.text())
    .then(logStatus)
    .catch(() => logStatus('Connection error'));
}

// Base controls
var baseDir = null;
function baseStart(dir) {
  if (baseDir === dir) return;
  baseDir = dir;
  var speed = document.getElementById('speedSlider').value;
  fetch('/base?action=' + dir + '&speed=' + speed).then(r => r.text()).then(logStatus).catch(() => logStatus('Error'));
  updateBaseUI(dir);
}
function baseStop() {
  if (baseDir === null) return;
  baseDir = null;
  fetch('/base?action=stop').then(r => r.text()).then(logStatus).catch(() => logStatus('Error'));
  updateBaseUI(null);
}
function updateBaseUI(dir) {
  var ccw = document.getElementById('btnCcw'), cw = document.getElementById('btnCw'), ind = document.getElementById('baseIndicator');
  ccw.classList.toggle('active', dir === 'ccw'); 
  cw.classList.toggle('active', dir === 'cw');
  if (dir === 'cw') { ind.innerText = 'Rotating CW &rarr;'; ind.classList.add('spinning'); }
  else if (dir === 'ccw') { ind.innerText = '&larr; Rotating CCW'; ind.classList.add('spinning'); }
  else { ind.innerText = 'Base Stopped'; ind.classList.remove('spinning'); }
}

function bindHold(el, dir) {
  el.addEventListener('pointerdown', e => { e.preventDefault(); baseStart(dir); });
  el.addEventListener('pointerup', baseStop);
  el.addEventListener('pointerleave', baseStop);
  el.addEventListener('pointercancel', baseStop);
}
bindHold(document.getElementById('btnCw'), 'cw');
bindHold(document.getElementById('btnCcw'), 'ccw');

// Presets & Automation
function setPreset(s1, s2, s3, s4, s5) {
  var angles = [s1, s2, s3, s4, s5];
  for (let i = 1; i <= 5; i++) {
    document.getElementById('slider' + i).value = angles[i-1];
    document.getElementById('val' + i).innerText = angles[i-1];
  }
  // Send sequential requests for preset joints
  angles.forEach((ang, idx) => {
    fetch('/servo?ch=' + (idx+1) + '&angle=' + ang);
  });
  logStatus('Preset applied successfully');
}

function home() {
  baseStop();
  fetch('/home').then(r => r.text()).then(t => {
    logStatus(t);
    for (var i = 1; i <= 5; i++) { 
      document.getElementById('slider' + i).value = 90; 
      document.getElementById('val' + i).innerText = 90; 
    }
  }).catch(() => logStatus('Connection error'));
}

function emergencyStop() {
  baseStop();
  fetch('/base?action=stop');
  logStatus('EMERGENCY STOP TRIGGERED');
}

function logStatus(text) {
  document.getElementById('logger').innerText = text;
}
</script>
</body>
</html>
)rawliteral");
}