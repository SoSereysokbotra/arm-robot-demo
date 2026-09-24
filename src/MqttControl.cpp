#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "MqttControl.h"
#include "Servos.h"
#include "Config.h"

// ---------- Topics ----------
static const char* T_STATUS     = MQTT_BASE_TOPIC "/status";      // online / offline (retained)
static const char* T_STATE      = MQTT_BASE_TOPIC "/state";       // {"1":90,"2":120,...}
static const char* T_LOG        = MQTT_BASE_TOPIC "/log";         // human-readable replies
static const char* T_SERVO_SUB  = MQTT_BASE_TOPIC "/servo/+";     // arm/servo/<ch>  payload: angle
static const char* T_SERVO_PREF = MQTT_BASE_TOPIC "/servo/";
static const char* T_BASE       = MQTT_BASE_TOPIC "/base";        // payload: cw | ccw | stop
static const char* T_BASE_SPEED = MQTT_BASE_TOPIC "/base/speed";  // payload: 0-100
static const char* T_BASE_TRIM  = MQTT_BASE_TOPIC "/base/trim";   // payload: neutral pulse, counts
static const char* T_HOME       = MQTT_BASE_TOPIC "/home";        // any payload

static WiFiClient wifiClient;
static PubSubClient mqtt(wifiClient);

static int baseSpeedPercent = 60;          // remembered between cw/ccw commands
static unsigned long lastReconnectAttempt = 0;
static unsigned long lastStatePublish = 0;
static float lastPublishedAngle[TOTAL_SERVOS];

// ---------- Helpers ----------
static void logMsg(const String& msg) {
  Serial.println(msg);
  if (mqtt.connected()) mqtt.publish(T_LOG, msg.c_str());
}

static void publishState(bool force = false) {
  bool changed = force;
  for (int ch = 1; ch < TOTAL_SERVOS && !changed; ch++) {
    if (currentAngle[ch] != lastPublishedAngle[ch]) changed = true;
  }
  if (!changed) return;

  String json = "{";
  for (int ch = 1; ch < TOTAL_SERVOS; ch++) {
    json += "\"" + String(ch) + "\":" + String((int)currentAngle[ch]);
    if (ch < TOTAL_SERVOS - 1) json += ",";
    lastPublishedAngle[ch] = currentAngle[ch];
  }
  json += "}";
  mqtt.publish(T_STATE, json.c_str());
}

static void applyBase(const String& action) {
  if (action == "cw") {
    setBaseSpeed(baseSpeedPercent);
    logMsg("OK: base rotating CW at " + String(baseSpeedPercent) + "%");
  } else if (action == "ccw") {
    setBaseSpeed(-baseSpeedPercent);
    logMsg("OK: base rotating CCW at " + String(baseSpeedPercent) + "%");
  } else if (action == "stop") {
    setBaseSpeed(0);
    logMsg("OK: base stopped");
  } else {
    logMsg("ERROR: base action must be cw, ccw, or stop");
  }
}

// ---------- Incoming message handler ----------
static void onMessage(char* topicC, byte* payloadB, unsigned int len) {
  String topic(topicC);
  String payload;
  payload.reserve(len);
  for (unsigned int i = 0; i < len; i++) payload += (char)payloadB[i];
  payload.trim();

  digitalWrite(STATUS_LED, HIGH);

  if (topic == T_HOME) {
    homeServos();
    logMsg("Homing all joints to home pose and stopping base.");
  }
  else if (topic == T_BASE_TRIM) {
    setBaseTrim(payload.toInt());
    logMsg("OK: base trim set to " + String(getBaseTrim()) + " counts");
  }
  else if (topic == T_BASE_SPEED) {
    baseSpeedPercent = constrain(payload.toInt(), 0, 100);
    logMsg("OK: base speed set to " + String(baseSpeedPercent) + "%");
  }
  else if (topic == T_BASE) {
    applyBase(payload);
  }
  else if (topic.startsWith(T_SERVO_PREF)) {
    int ch = topic.substring(strlen(T_SERVO_PREF)).toInt();
    int angle = payload.toInt();
    if (ch == BASE_SERVO) {
      logMsg("ERROR: base is continuous-rotation, use " + String(T_BASE) + " instead.");
    } else if (ch > 0 && ch < TOTAL_SERVOS && angle >= 0 && angle <= 180) {
      targetAngle[ch] = angle;
      logMsg("OK: Servo CH" + String(ch) + " moving to " + String(angle) + " degrees.");
    } else {
      logMsg("ERROR: invalid servo command (ch=" + String(ch) + ", angle=" + String(angle) + ")");
    }
  }

  digitalWrite(STATUS_LED, LOW);
}

// ---------- Connection management (non-blocking) ----------
static bool mqttReconnect() {
  Serial.print("Connecting to MQTT broker " MQTT_HOST "... ");
  // Last Will: broker publishes "offline" if the ESP32 disappears
  bool ok = mqtt.connect(MQTT_CLIENT_ID, T_STATUS, 0, true, "offline");
  if (!ok) {
    Serial.print("failed, rc=");
    Serial.println(mqtt.state());
    return false;
  }
  Serial.println("connected.");
  mqtt.publish(T_STATUS, "online", true);
  mqtt.subscribe(T_SERVO_SUB);
  mqtt.subscribe(T_BASE);
  mqtt.subscribe(T_BASE_SPEED);
  mqtt.subscribe(T_BASE_TRIM);
  mqtt.subscribe(T_HOME);
  publishState(true);
  logMsg("Robot arm ready. IP: " + WiFi.localIP().toString());
  return true;
}

void mqttSetup() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("Connecting to Wi-Fi \"" WIFI_SSID "\"...");

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMessage);
  mqtt.setBufferSize(512);

  for (int ch = 0; ch < TOTAL_SERVOS; ch++) lastPublishedAngle[ch] = -1;
}

void mqttLoop() {
  static bool wifiWasConnected = false;
  bool wifiUp = (WiFi.status() == WL_CONNECTED);

  if (wifiUp && !wifiWasConnected) {
    Serial.print("Wi-Fi connected, IP: ");
    Serial.println(WiFi.localIP());
  }
  wifiWasConnected = wifiUp;
  if (!wifiUp) return;   // WiFi auto-reconnects in the background

  if (!mqtt.connected()) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 5000) {   // retry every 5 s, never block
      lastReconnectAttempt = now;
      mqttReconnect();
    }
    return;
  }

  mqtt.loop();

  // Publish joint angles at most every 250 ms, only when something moved
  if (millis() - lastStatePublish > 250) {
    lastStatePublish = millis();
    publishState();
  }
}
