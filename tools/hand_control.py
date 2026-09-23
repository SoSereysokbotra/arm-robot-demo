#!/usr/bin/env python3
"""
Hand-gesture control for the robot arm: webcam -> MediaPipe -> MQTT -> ESP32.

    pip install mediapipe opencv-python paho-mqtt
    python tools/hand_control.py                 # broker = 192.168.1.194 (see --host)
    python tools/hand_control.py --dry-run       # camera + overlay only, nothing sent

ONE FINGER = ONE SERVO, "jog" style (like a robot teach pendant):

    bend a finger      -> its servo moves slowly           (stops when you straighten it)
    bend it again      -> it moves the OTHER way           (arrow on screen shows next direction)
    pinky              -> gripper toggles open / closed on each bend
    hand far left/right-> base CCW / CW                     (optional, see BASE_CONTROL)

    thumb -> CH1 shoulder   index -> CH2 elbow   middle -> CH3 wrist roll
    ring  -> CH4 wrist pitch pinky -> CH5 gripper

Keys:  SPACE pause/resume    H home    Q quit
"""
import argparse
import json
import math
import os
import time

import cv2
import paho.mqtt.client as mqtt
from mediapipe.tasks.python import BaseOptions
from mediapipe.tasks.python.vision import (HandLandmarker, HandLandmarkerOptions,
                                           HandLandmarksConnections, RunningMode)
import mediapipe as mp

# ------------------------------------------------------------------ tuning ----
MODEL = os.path.join(os.path.dirname(os.path.abspath(__file__)), "models", "hand_landmarker.task")
PREFIX = "arm"

# finger -> (servo channel, label, min angle, max angle)   -- keep these SAFE for your arm
FINGERS = {
    "thumb":  (1, "shoulder",    60, 170),
    "index":  (2, "elbow",       60, 170),
    "middle": (3, "wrist roll",  30, 150),
    "ring":   (4, "wrist pitch", 30, 150),
}
GRIPPER_CH, GRIP_OPEN, GRIP_CLOSED = 5, 20, 90      # pinky toggles between these

JOG_SPEED_DEG_S = 45            # how fast a servo moves while the finger is bent
CURL_ON, CURL_OFF = 0.65, 0.35  # hysteresis: finger counts as bent above ON, straight below OFF

# Bend angle (deg at middle joint) that counts as fully straight / fully bent
BEND_RANGE = {"index": (15, 110), "middle": (15, 110), "ring": (15, 110), "pinky": (15, 110)}
# Thumb: distance from thumb tip to PINKY knuckle, in palm-lengths (far = open, near = folded across palm)
THUMB_RANGE = (1.25, 0.70)

BASE_CONTROL   = True           # False = never touch the base from this script
BASE_SPEED     = 40             # % speed used for CW/CCW
BASE_DEADZONE  = 0.30           # +/- around centre (0..0.5) where the base stays stopped

SEND_INTERVAL_S = 0.10          # per-channel send rate limit
HAND_LOST_S     = 0.5           # stop the base if no hand for this long

# MediaPipe landmark indices: (base joint, middle joint, tip) per finger
JOINTS = {"index": (5, 6, 8), "middle": (9, 10, 12), "ring": (13, 14, 16), "pinky": (17, 18, 20)}
WRIST, THUMB_TIP, INDEX_MCP, MIDDLE_MCP, PINKY_MCP = 0, 4, 5, 9, 17
TIP = {"thumb": 4, "index": 8, "middle": 12, "ring": 16, "pinky": 20}


# --------------------------------------------------------------- helpers -----
def clamp(v, lo, hi):
    return max(lo, min(hi, v))


def lerp01(v, v_lo, v_hi):
    return clamp((v - v_lo) / (v_hi - v_lo), 0.0, 1.0)


def dist(a, b):
    return math.hypot(a.x - b.x, a.y - b.y)


def bend_angle(lm, base, mid, tip):
    """Angle (deg) at the middle joint between base->mid and mid->tip. 0 = straight."""
    v1 = (lm[base].x - lm[mid].x, lm[base].y - lm[mid].y, lm[base].z - lm[mid].z)
    v2 = (lm[tip].x - lm[mid].x, lm[tip].y - lm[mid].y, lm[tip].z - lm[mid].z)
    n1 = math.sqrt(sum(c * c for c in v1)) + 1e-9
    n2 = math.sqrt(sum(c * c for c in v2)) + 1e-9
    cos = clamp(sum(a * b for a, b in zip(v1, v2)) / (n1 * n2), -1.0, 1.0)
    return 180.0 - math.degrees(math.acos(cos))


def finger_curls(lm):
    """0 = straight .. 1 = bent, for all five fingers."""
    curls = {}
    for name, (b, m, t) in JOINTS.items():
        curls[name] = lerp01(bend_angle(lm, b, m, t), *BEND_RANGE[name])
    palm = dist(lm[WRIST], lm[MIDDLE_MCP]) + 1e-6
    curls["thumb_raw"] = dist(lm[THUMB_TIP], lm[PINKY_MCP]) / palm
    curls["thumb"] = lerp01(curls["thumb_raw"], *THUMB_RANGE)
    return curls


class Publisher:
    """Rate-limited MQTT sender that also tracks the arm's reported angles."""

    def __init__(self, host, port, dry_run):
        self.dry = dry_run
        self.last_val, self.last_t = {}, {}
        self.arm_state = {}          # {ch: angle} from arm/state
        self.client = None
        if not dry_run:
            try:
                self.client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
            except AttributeError:
                self.client = mqtt.Client()
            self.client.on_connect = lambda c, *_: c.subscribe(f"{PREFIX}/state")
            self.client.on_message = self._on_state
            self.client.connect(host, port, keepalive=30)
            self.client.loop_start()

    def _on_state(self, _c, _u, msg):
        try:
            self.arm_state = {int(k): float(v) for k, v in json.loads(msg.payload).items()}
        except (ValueError, AttributeError):
            pass

    def send(self, topic, value, force=False):
        full = f"{PREFIX}/{topic}"
        now = time.time()
        if not force and (self.last_val.get(full) == value or now - self.last_t.get(full, 0) < SEND_INTERVAL_S):
            return
        self.last_val[full], self.last_t[full] = value, now
        if self.dry:
            print(f"[dry] {full}: {value}")
        else:
            self.client.publish(full, str(value))

    def close(self):
        if self.client:
            self.client.loop_stop()
            self.client.disconnect()


# ---------------------------------------------------------------- drawing ----
def put(frame, text, x, y, scale=0.6, color=(80, 255, 80)):
    cv2.putText(frame, text, (x, y), cv2.FONT_HERSHEY_SIMPLEX, scale, (0, 0, 0), 4)
    cv2.putText(frame, text, (x, y), cv2.FONT_HERSHEY_SIMPLEX, scale, color, 1)


def draw_hand(frame, lm, curls, bent, direction, grip_closed):
    h, w = frame.shape[:2]
    pts = [(int(p.x * w), int(p.y * h)) for p in lm]
    for c in HandLandmarksConnections.HAND_CONNECTIONS:
        cv2.line(frame, pts[c.start], pts[c.end], (0, 200, 255), 2)
    for p in pts:
        cv2.circle(frame, p, 4, (255, 255, 255), -1)
    for name, (ch, label, _, _) in FINGERS.items():
        tip = pts[TIP[name]]
        active = bent[name]
        color = (0, 0, 255) if active else (0, 200, 0)
        cv2.circle(frame, tip, 10 if active else 7, color, -1)
        arrow = "+" if direction[name] > 0 else "-"
        put(frame, f"CH{ch} {label} {arrow}", tip[0] + 12, tip[1] - 6, 0.5, color)
    tip = pts[TIP["pinky"]]
    color = (0, 0, 255) if bent["pinky"] else (0, 200, 0)
    cv2.circle(frame, tip, 10 if bent["pinky"] else 7, color, -1)
    put(frame, f"CH{GRIPPER_CH} gripper {'CLOSED' if grip_closed else 'OPEN'}", tip[0] + 12, tip[1] - 6, 0.5, color)


def draw_hud(frame, lines, paused):
    h, w = frame.shape[:2]
    for i, text in enumerate(lines):
        put(frame, text, 12, 28 + i * 26)
    if BASE_CONTROL:
        xl, xr = int(w * (0.5 - BASE_DEADZONE)), int(w * (0.5 + BASE_DEADZONE))
        for x in (xl, xr):
            cv2.line(frame, (x, 0), (x, h), (90, 90, 90), 1)
        for text, x in (("<- BASE CCW", 10), ("BASE STOP", xl + 10), ("BASE CW ->", xr + 10)):
            put(frame, text, x, h - 50, 0.55, (200, 200, 200))
    put(frame, "BEND finger = servo moves (+/-)   STRAIGHT = stop   bend again = other way   PINKY = gripper toggle",
        10, h - 80, 0.45, (200, 200, 200))
    if paused:
        put(frame, "PAUSED  (SPACE to resume)", 12, h - 16, 0.8, (0, 0, 255))


# ------------------------------------------------------------------ main -----
def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", default="192.168.1.194")
    ap.add_argument("--port", type=int, default=1883)
    ap.add_argument("--camera", type=int, default=0)
    ap.add_argument("--no-mirror", action="store_true", help="don't flip the camera image")
    ap.add_argument("--dry-run", action="store_true", help="show camera + overlay, send nothing")
    a = ap.parse_args()

    pub = Publisher(a.host, a.port, a.dry_run)
    landmarker = HandLandmarker.create_from_options(HandLandmarkerOptions(
        base_options=BaseOptions(model_asset_path=MODEL), running_mode=RunningMode.VIDEO,
        num_hands=1, min_hand_detection_confidence=0.6, min_tracking_confidence=0.6))

    cap = cv2.VideoCapture(a.camera, cv2.CAP_DSHOW)
    if not cap.isOpened():
        raise SystemExit(f"Could not open camera {a.camera}")

    # start from the arm's real angles if we hear them within 1 s, else mid-range
    time.sleep(1.0)
    angle = {}
    for name, (ch, _, lo, hi) in FINGERS.items():
        angle[name] = clamp(pub.arm_state.get(ch, (lo + hi) / 2), lo, hi)
    grip_closed = pub.arm_state.get(GRIPPER_CH, GRIP_OPEN) > (GRIP_OPEN + GRIP_CLOSED) / 2

    bent = {n: False for n in list(FINGERS) + ["pinky"]}
    direction = {n: +1 for n in FINGERS}
    paused = False
    base_state = "stop"
    last_hand_t = 0.0
    t0 = time.time()
    prev_t = t0

    if BASE_CONTROL:
        pub.send("base/speed", BASE_SPEED, force=True)
    print("Running.  Bend a finger to jog its servo.  SPACE = pause, H = home, Q = quit")

    while True:
        ok, frame = cap.read()
        if not ok:
            break
        if not a.no_mirror:
            frame = cv2.flip(frame, 1)
        rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        now = time.time()
        dt, prev_t = now - prev_t, now
        res = landmarker.detect_for_video(mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb), int((now - t0) * 1000))

        hud = []
        if res.hand_landmarks:
            lm = res.hand_landmarks[0]
            last_hand_t = now
            curls = finger_curls(lm)

            # --- bent / straight with hysteresis
            for n in bent:
                was = bent[n]
                bent[n] = curls[n] > CURL_ON if not was else curls[n] > CURL_OFF
                if bent[n] and not was:                       # just bent
                    if n == "pinky":
                        grip_closed = not grip_closed
                        if not paused:
                            pub.send(f"servo/{GRIPPER_CH}", GRIP_CLOSED if grip_closed else GRIP_OPEN, force=True)
                    else:
                        ch, _, lo, hi = FINGERS[n]
                        # if we're sitting at a limit, only one direction makes sense
                        if angle[n] >= hi: direction[n] = -1
                        elif angle[n] <= lo: direction[n] = +1
                elif not bent[n] and was and n != "pinky":     # just released -> next bend goes the other way
                    direction[n] *= -1

            # --- jog the servos of bent fingers
            for n, (ch, _, lo, hi) in FINGERS.items():
                if bent[n] and not paused:
                    angle[n] = clamp(angle[n] + direction[n] * JOG_SPEED_DEG_S * dt, lo, hi)
                    pub.send(f"servo/{ch}", int(round(angle[n])))

            draw_hand(frame, lm, curls, bent, direction, grip_closed)

            cx = lm[MIDDLE_MCP].x
            if not BASE_CONTROL:
                want_base = "stop"
            elif cx < 0.5 - BASE_DEADZONE:
                want_base = "ccw"
            elif cx > 0.5 + BASE_DEADZONE:
                want_base = "cw"
            else:
                want_base = "stop"
            if not paused and want_base != base_state:
                base_state = want_base
                pub.send("base", base_state, force=True)

            hud.append("   ".join(f"CH{ch} {angle[n]:.0f}" for n, (ch, _, _, _) in FINGERS.items())
                       + f"   grip {'CLOSED' if grip_closed else 'OPEN'}   base {want_base}")
            hud.append("curl  " + "  ".join(f"{n[:2]} {curls[n]:.2f}" for n in ("thumb", "index", "middle", "ring", "pinky"))
                       + f"   (thumb raw {curls['thumb_raw']:.2f})")
        else:
            hud.append("no hand")
            for n in bent: bent[n] = False
            if base_state != "stop" and now - last_hand_t > HAND_LOST_S:
                base_state = "stop"
                pub.send("base", "stop", force=True)

        draw_hud(frame, hud, paused)
        cv2.imshow("Hand control  -  SPACE pause  H home  Q quit", frame)
        key = cv2.waitKey(1) & 0xFF
        if key in (ord("q"), 27):
            break
        elif key == ord(" "):
            paused = not paused
            if paused and base_state != "stop":
                base_state = "stop"
                pub.send("base", "stop", force=True)
        elif key == ord("h"):
            pub.send("home", 1, force=True)
            time.sleep(1.0)                                   # let the arm report its new angles
            for n, (ch, _, lo, hi) in FINGERS.items():
                angle[n] = clamp(pub.arm_state.get(ch, angle[n]), lo, hi)

    if base_state != "stop":
        pub.send("base", "stop", force=True)
    cap.release()
    cv2.destroyAllWindows()
    pub.close()


if __name__ == "__main__":
    main()
