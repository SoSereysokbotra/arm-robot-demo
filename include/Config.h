#ifndef CONFIG_H
#define CONFIG_H

// Wi-Fi credentials and the broker IP live in Secrets.h, which is gitignored.
// First time on a new machine: copy include/Secrets.example.h to include/Secrets.h
// and fill it in. It defines WIFI_SSID, WIFI_PASSWORD and MQTT_HOST.
#include "Secrets.h"

// ---------- MQTT broker ----------
#define MQTT_PORT      1883
#define MQTT_CLIENT_ID "robot-arm-esp32"

// Topic prefix: everything the arm listens to / publishes starts with this
#define MQTT_BASE_TOPIC "arm"

// PCA9685 Settings
#define SERVO_FREQ 50
const int SERVOMIN = 150;
const int SERVOMAX = 600;
const int TOTAL_SERVOS = 6;

const int BASE_SERVO = 0;

// ---------- Continuous-rotation base (CH0) ----------
// The positional servos are driven through the SERVOMIN..SERVOMAX map above,
// but that map puts "neutral" nowhere near the 1500 us a continuous-rotation
// servo needs, which made CCW impossible at low speeds. The base therefore gets
// its own pulse maths, in raw PCA9685 counts (1 count = 20000us/4096 = 4.88us).
const int BASE_PULSE_STOP = 307;   // ~1500 us - the servo standing still
const int BASE_PULSE_SPAN = 110;   // ~+/-537 us at 100 % - full speed either way
// If CW and CCW run at noticeably different speeds, nudge BASE_PULSE_STOP.

// Home / startup pose (degrees). Index = PCA9685 channel.
// CH0 is the continuous-rotation base (value unused, kept for alignment).
//                              CH0  CH1       CH2    CH3      CH4     CH5
//                              base shoulder elbow  wristP   wristR  gripper
const int HOME_POSE[TOTAL_SERVOS] = { 90, 151,     153,   54,      57,     90 };

// Motion Smoothing
const unsigned long STEP_INTERVAL_US = 8000;
const float STEP_SIZE_DEG = 1.0;

// ESP32 Pins
const int STATUS_LED = 13;
const int BUZZER_PIN = 14;
const int I2C_SDA = 21;
const int I2C_SCL = 22;

#endif
