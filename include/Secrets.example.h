#ifndef SECRETS_H
#define SECRETS_H

// Template. Copy this file to include/Secrets.h and fill in your own values.
// Secrets.h is gitignored, so your credentials never leave your machine.

// 1 = the arm makes its own Wi-Fi and needs no router at all. Join "RobotArm"
//     from the laptop, run Mosquitto there, and the arm finds it. Use this when
//     there is no 2.4 GHz network around - the ESP32 cannot see 5 GHz.
// 0 = the arm joins the network named below.
#define USE_AP_MODE   1

// Wi-Fi the ESP32 joins (2.4 GHz only - the ESP32 cannot see 5 GHz networks)
#define WIFI_SSID     "YOUR_WIFI_NAME"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// MQTT broker. A public cloud broker means the arm and the page need no
// shared network - only internet access. Put a LAN IP here to go back to
// a local Mosquitto.
#define MQTT_HOST     "broker.emqx.io"   // public EMQX broker, reachable from any network

#endif
