#!/usr/bin/env python3
"""
Teach-and-replay pick and place.

Nobody can guess the joint angles for "the box on the table" - they depend on
this arm's link lengths and where the box actually is. So you teach each
position once by jogging the arm, and this replays them, dropping into a
randomly chosen bin each cycle.

    pip install paho-mqtt

Teach (jog the arm in the 3D app, then press Enter to capture):

    python tools/pick_place.py --teach home
    python tools/pick_place.py --teach pick_above
    python tools/pick_place.py --teach pick
    python tools/pick_place.py --teach drop_a
    python tools/pick_place.py --teach drop_b
    python tools/pick_place.py --teach drop_c

Check and run:

    python tools/pick_place.py --list
    python tools/pick_place.py --dry-run        # print the moves, touch nothing
    python tools/pick_place.py --run            # one cycle
    python tools/pick_place.py --run --loop 5   # five cycles

Positions live in tools/poses.json, so they survive between sessions.
"""
import argparse
import json
import os
import random
import sys
import time

import paho.mqtt.client as mqtt

PREFIX = "arm"
POSES_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "poses.json")

JOINTS = {1: "shoulder", 2: "elbow", 3: "wrist roll", 4: "wrist pitch"}
GRIPPER_CH = 5
GRIP_OPEN, GRIP_CLOSED = 20, 90        # must match the app's gripper buttons

# A cycle is: go above the box, descend, grip, lift, go above a bin, descend,
# release, lift, return home. Any position missing from poses.json is skipped,
# so you can start with just pick and one drop point.
SETTLE = 1.4                            # seconds to let a move finish
GRIP_SETTLE = 0.9


class Arm:
    def __init__(self, host, port, dry):
        self.dry = dry
        self.state = {}
        self.client = None
        if dry:
            return
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
            self.state = {int(k): int(float(v)) for k, v in json.loads(msg.payload).items()}
        except (ValueError, TypeError):
            pass

    def send(self, ch, angle):
        if self.dry:
            print(f"    servo {ch} -> {angle}")
        else:
            self.client.publish(f"{PREFIX}/servo/{ch}", str(int(angle)))

    def goto(self, name, pose, settle=SETTLE):
        shown = {c: v for c, v in pose.items() if int(c) != GRIPPER_CH}
        print(f"  {name}: " + "  ".join(f"CH{c}={v}" for c, v in sorted(shown.items())))
        for ch, angle in sorted(pose.items()):
            if int(ch) == GRIPPER_CH:
                continue            # the gripper is driven by grip(), not by poses
            self.send(int(ch), angle)
            time.sleep(0.05)            # don't flood the broker in one burst
        time.sleep(settle)

    def grip(self, close):
        print("  gripper:", "close" if close else "open")
        self.send(GRIPPER_CH, GRIP_CLOSED if close else GRIP_OPEN)
        time.sleep(GRIP_SETTLE)

    def wait_for_state(self, timeout=5):
        end = time.time() + timeout
        while time.time() < end:
            if self.state:
                return True
            time.sleep(0.2)
        return False

    def close(self):
        if self.client:
            self.client.loop_stop()
            self.client.disconnect()


# ------------------------------------------------------------------ poses ---
def load_poses():
    if not os.path.exists(POSES_FILE):
        return {}
    try:
        with open(POSES_FILE, encoding="utf-8") as f:
            return json.load(f)
    except (OSError, ValueError):
        return {}


def save_poses(poses):
    with open(POSES_FILE, "w", encoding="utf-8") as f:
        json.dump(poses, f, indent=2, sort_keys=True)


def teach(arm, name):
    print(f"\nTeaching '{name}'.")
    print("Jog the arm in the 3D app until it is exactly where you want it.")
    print("The gripper is captured too, so set that as well.")
    input("Press Enter to capture (Ctrl+C to abort)... ")

    if not arm.wait_for_state():
        sys.exit("No telemetry from the arm. Is it online and the broker running?")

    pose = {str(ch): arm.state[ch] for ch in sorted(arm.state) if ch in JOINTS}
    if GRIPPER_CH in arm.state:
        pose[str(GRIPPER_CH)] = arm.state[GRIPPER_CH]

    poses = load_poses()
    poses[name] = pose
    save_poses(poses)
    print(f"Saved '{name}': " + "  ".join(f"CH{c}={v}" for c, v in sorted(pose.items())))
    print(f"-> {POSES_FILE}")


# ------------------------------------------------------------------ cycle ---
def run_cycle(arm, poses, n):
    drops = sorted(k for k in poses
                   if k.startswith("drop") and not k.endswith("_above"))
    if "pick" not in poses:
        sys.exit("No 'pick' position taught yet. Run:  --teach pick")
    if not drops:
        sys.exit("No drop position taught yet. Run:  --teach drop_a")

    target = random.choice(drops)
    print(f"\n--- cycle {n}: dropping at '{target}' ---")

    def step(key, settle=SETTLE):
        if key in poses:
            arm.goto(key, poses[key], settle)

    arm.grip(False)                       # open before approaching
    step("pick_above")
    step("pick")
    arm.grip(True)                        # grab the box
    step("pick_above")                    # lift straight up

    above = target + "_above"
    step(above)
    step(target)
    arm.grip(False)                       # release
    step(above)
    step("home")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", default="localhost", help="MQTT broker (default: localhost)")
    ap.add_argument("--port", type=int, default=1883)
    ap.add_argument("--teach", metavar="NAME",
                    help="capture the arm's current pose under this name")
    ap.add_argument("--run", action="store_true", help="run pick and place")
    ap.add_argument("--loop", type=int, default=1, help="how many cycles (default 1)")
    ap.add_argument("--list", action="store_true", help="show the taught positions")
    ap.add_argument("--dry-run", action="store_true", help="print the moves, send nothing")
    a = ap.parse_args()

    if a.list:
        poses = load_poses()
        if not poses:
            print("Nothing taught yet.")
        for name, pose in sorted(poses.items()):
            print(f"{name:14} " + "  ".join(f"CH{c}={v}" for c, v in sorted(pose.items())))
        return

    arm = Arm(a.host, a.port, a.dry_run)
    try:
        if a.teach:
            teach(arm, a.teach)
            return
        if not a.run and not a.dry_run:
            ap.print_help()
            return

        poses = load_poses()
        for i in range(1, a.loop + 1):
            run_cycle(arm, poses, i)
        print("\nDone.")
    except KeyboardInterrupt:
        print("\nstopped")
    finally:
        arm.close()


if __name__ == "__main__":
    main()
