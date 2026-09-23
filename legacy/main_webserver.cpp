#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include "Config.h"
#include "Servos.h"
#include "WebServer.h"
//192.168.4.1

// Wi-Fi AP credentials
const char* AP_SSID = "Arm robot";
const char* AP_PASSWORD = "arm_robot123";

// Create server on port 80
WiFiServer server(80);

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Init pins & I2C communication
  pinMode(STATUS_LED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  Wire.begin(I2C_SDA, I2C_SCL);

  // Init PCA9685 driver & reset joints
  initServos();

  // Start Wi-Fi Access Point
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  // Start HTTP server
  server.begin();
}

void loop() {
  // 1. Keep moving servos smoothly every cycle
  stepServos();

  // 2. Check for incoming phone/browser connection
  WiFiClient client = server.available();
  if (!client) return;

  // Wait for request data (with timeout)
  unsigned long waitStart = millis();
  while (!client.available()) {
    stepServos(); // Keep easing while waiting
    if (millis() - waitStart > 3000) { client.stop(); return; }
    delay(1);
  }

  // 3. Read HTTP request line
  String request = client.readStringUntil('\r');
  while (client.available()) { client.read(); }

  // 4. Process command (move servo, base, home, etc.)
  handleWebRequest(client, request);

  // 5. Close connection safely
  client.flush();
  delay(1);
  client.stop();
}