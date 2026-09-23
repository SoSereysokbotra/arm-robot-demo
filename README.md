# Robot Arm Control (ESP32 + PCA9685 + MQTT + Node-RED)

```
Node-RED dashboard â”€â”€MQTTâ”€â”€â–¶ Mosquitto broker â”€â”€MQTTâ”€â”€â–¶ ESP32 â”€â”€I2Câ”€â”€â–¶ PCA9685 â”€â”€â–¶ 6 servos
 (phone/laptop browser)        (your PC)                 (Wi-Fi)
```

## Project layout

| Path | What |
|---|---|
| `src/main.cpp` | `setup()` / `loop()` |
| `src/Servos.cpp` | PCA9685 driver + smooth non-blocking easing |
| `src/MqttControl.cpp` | Wi-Fi + MQTT: receives commands, publishes state |
| `include/Config.h` | **Wi-Fi name/password, broker IP**, pins, servo limits |
| `nodered/flow.json` | Dashboard â€” import into Node-RED |
| `nodered/mosquitto.conf` | Broker config that allows LAN connections |
| `tools/arm_ctl.py` | Python CLI for testing / scripting (`pip install paho-mqtt`) |
| `tools/hand_control.py` | Webcam hand-gesture control (MediaPipe -> MQTT) |
| `sim/arm3d.html` | 3D simulation (digital twin) that mirrors the real arm |
| `tools/models/hand_landmarker.task` | MediaPipe hand model used by `hand_control.py` |
| `legacy/` | Old Wi-Fi AP + HTML web server version (not compiled) |

## MQTT topics

| Topic | Payload | Direction |
|---|---|---|
| `arm/servo/1` â€¦ `arm/servo/5` | `0`â€“`180` | â†’ ESP32 |
| `arm/base` | `cw` / `ccw` / `stop` | â†’ ESP32 |
| `arm/base/speed` | `0`â€“`100` | â†’ ESP32 |
| `arm/home` | anything | â†’ ESP32 |
| `arm/status` | `online` / `offline` (retained) | ESP32 â†’ |
| `arm/state` | `{"1":90,"2":120,...}` | ESP32 â†’ |
| `arm/log` | text reply | ESP32 â†’ |

## Setup (5 steps)

### 1. Start Mosquitto (broker)
Open an **Administrator** PowerShell:
```powershell
& "C:\Program Files\mosquitto\mosquitto.exe" -c "D:\Year2\Semester2\IoT\robot_arm_control\nodered\mosquitto.conf" -v
```
Leave it running. If Windows Firewall asks, allow it. Find your PC's IP with `ipconfig` (e.g. `192.168.1.100`).

> **Two brokers gotcha:** the Mosquitto installer also registers a Windows *service* that listens on
> `localhost` only. The ESP32 can't reach that one, but PC tools using `localhost` will — so messages
> silently go to different brokers. Stop it once (Admin PowerShell):
> `Stop-Service mosquitto; Set-Service mosquitto -StartupType Manual`
> Or always use your PC's LAN IP (not `localhost`) in `arm_ctl.py --host` and the Node-RED broker node.

### 2. Configure and flash the ESP32
Edit `include/Config.h`:
```cpp
#define WIFI_SSID     "your wifi"
#define WIFI_PASSWORD "your password"
#define MQTT_HOST     "192.168.1.100"   // PC IP from step 1
```
Then in VS Code: PlatformIO **â†’ Upload**, then **Serial Monitor**. You should see
`Wi-Fi connected` and `Connecting to MQTT broker ... connected.`

> The ESP32 and your PC must be on the **same Wi-Fi network**. A phone hotspot works fine.

### 3. Quick test without Node-RED (optional)
```powershell
pip install paho-mqtt
python tools\arm_ctl.py watch          # in one terminal
python tools\arm_ctl.py servo 3 120    # in another
python tools\arm_ctl.py demo
```

### 4. Install Node-RED + dashboard
```powershell
npm install -g --unsafe-perm node-red
node-red
```
Open http://localhost:1880 â†’ menu (â‰¡) â†’ **Manage palette** â†’ Install â†’ search `node-red-dashboard` â†’ Install.

### 5. Import the dashboard
Menu (â‰¡) â†’ **Import** â†’ select file `nodered/flow.json` â†’ Import â†’ click **Deploy**.
Open **http://localhost:1880/ui** (or `http://<PC-IP>:1880/ui` from your phone).

## Editing the UI
Everything is drag-and-drop in the Node-RED editor. To add a control, drag a
dashboard widget, set its *Topic* (e.g. `arm/servo/2`), wire it to the
`â†’ Mosquitto` node, Deploy. No re-flashing needed.

## Hand-gesture control (webcam)

```powershell
pip install mediapipe opencv-python paho-mqtt
python tools\hand_control.py --dry-run     # first: camera + overlay only, nothing is sent
python tools\hand_control.py               # live control
```

| Your hand | Arm |
|---|---|
| move left / right | base rotates CCW / CW (centre band = stop) |
| move up / down | shoulder (CH1) |
| move closer / farther | elbow (CH2) |
| fist / open hand | gripper close / open (CH5) |
| peace sign, hold 1.5 s | HOME pose |

Keys: **SPACE** pause/resume, **H** home, **Q** quit. The base stops automatically if the hand leaves the frame.

Tune the angle ranges, speeds and thresholds at the top of `tools/hand_control.py`
(`SHOULDER_RANGE`, `ELBOW_RANGE`, `GRIP_OPEN/CLOSED`, `BASE_SPEED`, `SMOOTHING`, ...).

## 3D simulation (digital twin)

`sim/arm3d.html` draws the arm in 3D and mirrors every movement of the real one,
by subscribing to the same `arm/state` topic the dashboard uses.

Browsers cannot open raw MQTT sockets, so Mosquitto needs a **WebSocket listener**.
That is already in `nodered/mosquitto.conf`:

```
listener 9001 0.0.0.0
protocol websockets
```

If Mosquitto is already running, stop it (Ctrl+C) and start it again so the new
listener is picked up. You should see BOTH lines in its output:

```
Opening ipv4 listen socket on port 1883.
Opening websockets listen socket on port 9001.
```

Then just double-click `sim/arm3d.html` (or open it in a browser). It connects to
`192.168.1.194:9001` by default; change the broker box at the bottom-left if your
PC's IP differs, or open it as `arm3d.html?host=192.168.1.50`.

Controls: **drag** to orbit, **wheel** to zoom, **right-drag** to pan.

What is shown:

| 3D joint | From |
|---|---|
| shoulder, elbow, wrist pitch, wrist roll, gripper | `arm/state` (real angles reported by the ESP32) |
| base rotation | integrated from `arm/base` + `arm/base/speed` &mdash; the continuous-rotation base has no position feedback, so this is an estimate |

If a joint bends the wrong way in 3D, set `invert: true` for it in the `JOINTS`
table at the top of the `<script>` block.
