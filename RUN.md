# Run checklist

Everything you need to start the robot arm, in order. Run each block in its own
PowerShell window and leave it open.

---

## 1. Start the MQTT broker (always first)

```powershell
& "C:\Program Files\mosquitto\mosquitto.exe" -c "D:\Year2\Semester2\IoT\robot_arm_control\nodered\mosquitto.conf" -v
```

You must see **both** of these lines, or the 3D app cannot connect:

```
Opening ipv4 listen socket on port 1883.
Opening websockets listen socket on port 9001.
```

Leave this window open. Ctrl+C stops the broker.

---

## 2. Upload the firmware (only when C++ in `src/` or `include/` changed)

Plug the ESP32 into USB first.

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -t upload --upload-port COM7
```

If the port has changed, find it with:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" device list
```

and use the one described as **USB-SERIAL CH340**.

Watch the boot messages (Ctrl+C to leave; close it before the next upload):

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" device monitor -p COM7 -b 115200
```

A good boot looks like:

```
Servos at home pose - waiting for commands.
Wi-Fi connected, IP: 192.168.1.80
Connecting to MQTT broker 192.168.1.194... connected.
```

---

## 3. Power the arm and open the app

The ESP32 only needs power — USB or its own supply. It joins Wi-Fi by itself.

Then open the control app:

```
D:\Year2\Semester2\IoT\robot_arm_control\sim\arm3d.html
```

Double-click it, or run:

```powershell
start "" "D:\Year2\Semester2\IoT\robot_arm_control\sim\arm3d.html"
```

Wait for the chip in the toolbar to turn green and read **LINKED**.

---

## 4. Check it before demoing (30 seconds)

| Step | Expected |
|---|---|
| Toolbar chip | green **LINKED** |
| Drag **J4 wrist pitch** | real wrist moves, 3D follows |
| **Gripper Open / Close** | real claw and 3D agree |
| Base: speed **30** then **CW** &rarr; **Stop** | base turns clockwise, then holds still |
| Base: **CCW** &rarr; **Stop** | base turns back at about the same speed |

Both base directions must work. If only one does, the firmware in step 2 has not
been uploaded, or `BASE_PULSE_STOP` in `include/Config.h` needs nudging.

Then press **Run demo**, and keep a hand near **Stop** the first time.

---

## Optional tools

Node-RED dashboard (an alternative to the 3D app):

```powershell
node-red
```

then open <http://localhost:1880/ui>

Command-line testing:

```powershell
cd D:\Year2\Semester2\IoT\robot_arm_control
python tools\arm_ctl.py watch          # print everything on the broker
python tools\arm_ctl.py servo 4 120    # move CH4 to 120 degrees
python tools\arm_ctl.py home
```

Webcam hand control:

```powershell
cd D:\Year2\Semester2\IoT\robot_arm_control
python tools\hand_control.py --dry-run   # camera only, sends nothing
python tools\hand_control.py             # live
```

---

## If something is wrong

| Symptom | Cause | Fix |
|---|---|---|
| App says **NO LINK** | broker not running, or started without `-c` | redo step 1, check both "Opening" lines |
| Chip green but arm does nothing | ESP32 not powered or not on Wi-Fi | check `arm/status` with `python tools\arm_ctl.py watch` — it should say `online` |
| `arm/status: offline` | ESP32 lost the broker | power-cycle the ESP32 |
| Base goes out but not back | old firmware on the ESP32 | redo step 2 |
| Base turns too far / not far enough | timing estimate is off | change `BASE_DEG_PER_SEC` near the top of the `<script>` in `sim/arm3d.html` |
| Servos buzz loudly | commanded past their mechanical stop, or supply too weak | cut power; servos need their own 5-6 V, 3 A supply into the PCA9685 `V+` |
| 3D viewport blank | script error | the **Activity** panel and the browser tab title show the message |
| Upload fails, "port is busy" | serial monitor still open | close it, then upload |
