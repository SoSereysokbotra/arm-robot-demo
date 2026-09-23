#!/usr/bin/env python3
"""
Command-line controller for the ESP32 robot arm over MQTT.
Useful for testing without Node-RED, or for scripting motion sequences.

Install:  pip install paho-mqtt
Usage:
  python arm_ctl.py servo 3 120        # move CH3 to 120 deg
  python arm_ctl.py base cw 60         # rotate base CW at 60 %
  python arm_ctl.py base stop
  python arm_ctl.py home
  python arm_ctl.py watch              # print everything the arm publishes
  python arm_ctl.py demo               # small wave sequence

Set the broker with --host (default: localhost).
"""
import argparse
import json
import sys
import time

import paho.mqtt.client as mqtt

PREFIX = "arm"


def make_client():
    try:  # paho-mqtt >= 2.0
        return mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    except AttributeError:  # paho-mqtt 1.x
        return mqtt.Client()


def publish(client, topic, payload):
    full = f"{PREFIX}/{topic}"
    info = client.publish(full, str(payload))
    info.wait_for_publish()
    print(f"-> {full}: {payload}", flush=True)


def cmd_watch(host, port):
    client = make_client()

    def on_connect(c, *_):
        c.subscribe(f"{PREFIX}/#")
        print(f"Watching {PREFIX}/# on {host}:{port}  (Ctrl+C to stop)", flush=True)

    def on_message(_c, _u, msg):
        print(f"<- {msg.topic}: {msg.payload.decode(errors='replace')}", flush=True)

    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(host, port, keepalive=30)
    client.loop_forever()


def cmd_demo(client):
    seq = [(1, 60), (2, 120), (3, 45), (5, 30), (5, 150), (1, 120), (2, 60), (3, 135)]
    for ch, angle in seq:
        publish(client, f"servo/{ch}", angle)
        time.sleep(1.2)
    publish(client, "home", 1)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", default="192.168.1.194", help="broker IP (your PC LAN IP, same as MQTT_HOST in Config.h)")
    ap.add_argument("--port", type=int, default=1883)
    ap.add_argument("cmd", choices=["servo", "base", "home", "watch", "demo"])
    ap.add_argument("args", nargs="*")
    a = ap.parse_args()

    if a.cmd == "watch":
        cmd_watch(a.host, a.port)
        return

    client = make_client()
    client.connect(a.host, a.port, keepalive=30)
    client.loop_start()

    if a.cmd == "servo":
        if len(a.args) != 2:
            sys.exit("usage: servo <channel 1-5> <angle 0-180>")
        publish(client, f"servo/{int(a.args[0])}", int(a.args[1]))
    elif a.cmd == "base":
        if not a.args or a.args[0] not in ("cw", "ccw", "stop"):
            sys.exit("usage: base <cw|ccw|stop> [speed 0-100]")
        if len(a.args) > 1:
            publish(client, "base/speed", int(a.args[1]))
        publish(client, "base", a.args[0])
    elif a.cmd == "home":
        publish(client, "home", 1)
    elif a.cmd == "demo":
        cmd_demo(client)

    time.sleep(0.2)
    client.loop_stop()
    client.disconnect()


if __name__ == "__main__":
    main()
