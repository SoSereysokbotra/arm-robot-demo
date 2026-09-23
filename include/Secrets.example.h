#ifndef SECRETS_H
#define SECRETS_H

// Template. Copy this file to include/Secrets.h and fill in your own values.
// Secrets.h is gitignored, so your credentials never leave your machine.

// Wi-Fi the ESP32 joins (2.4 GHz only - the ESP32 cannot see 5 GHz networks)
#define WIFI_SSID     "YOUR_WIFI_NAME"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// IP of the PC running Mosquitto, on that same network (run: ipconfig)
#define MQTT_HOST     "192.168.1.100"

#endif
