#include <Arduino.h>
#include <Wire.h>
#include "Config.h"
#include "Servos.h"
#include "MqttControl.h"

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Init pins & I2C communication
  pinMode(STATUS_LED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  Wire.begin(I2C_SDA, I2C_SCL);

  // Init PCA9685 driver & reset joints
  initServos();

  // Join Wi-Fi and start talking to the MQTT broker (non-blocking)
  mqttSetup();
}

void loop() {
  // 1. Keep moving servos smoothly every cycle
  stepServos();

  // 2. Handle Wi-Fi/MQTT: receive commands, publish state, reconnect if needed
  mqttLoop();
}
