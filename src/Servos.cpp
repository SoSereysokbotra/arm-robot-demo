#include "Servos.h"
#include "Config.h"

// Initialize PCA9685 driver object with I2C address 0x40
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

// Arrays to track motion state for each servo joint
float currentAngle[TOTAL_SERVOS];
int targetAngle[TOTAL_SERVOS];
unsigned long lastStepMicros[TOTAL_SERVOS];

// Setup PWM frequency and set default positions on boot
void initServos() {
  Serial.println("Initializing PCA9685...");
  pwm.begin();
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(SERVO_FREQ);
  Serial.println("PCA9685 initialized.");

  // Drive all positional joints (CH1-CH5) straight to the home pose on boot
  for (int ch = 1; ch < TOTAL_SERVOS; ch++) {
    currentAngle[ch] = HOME_POSE[ch];
    targetAngle[ch] = HOME_POSE[ch];
    lastStepMicros[ch] = 0;
    writeServoPulse(ch, currentAngle[ch]);
  }
  setBaseSpeed(0);
  Serial.println("Servos at home pose - waiting for commands.");
}

// Smoothly return every joint to HOME_POSE (uses the easing loop) and stop the base
void homeServos() {
  for (int ch = 1; ch < TOTAL_SERVOS; ch++) targetAngle[ch] = HOME_POSE[ch];
  setBaseSpeed(0);
}

// Non-blocking easing loop: smoothly steps joints toward their targets
void stepServos() {
  unsigned long now = micros();

  for (int ch = 1; ch < TOTAL_SERVOS; ch++) {
    // Check if it's time for the next step based on interval timer
    if (now - lastStepMicros[ch] < STEP_INTERVAL_US) continue;
    lastStepMicros[ch] = now;

    float diff = targetAngle[ch] - currentAngle[ch];

    // If close enough to target, snap to final angle and stop stepping
    if (fabs(diff) < STEP_SIZE_DEG) {
      if (diff != 0) {
        currentAngle[ch] = targetAngle[ch];
        writeServoPulse(ch, currentAngle[ch]);
      }
      continue;
    }

    // Take a small step (+/- 1 degree) toward the target angle
    currentAngle[ch] += (diff > 0 ? STEP_SIZE_DEG : -STEP_SIZE_DEG);
    writeServoPulse(ch, currentAngle[ch]);
  }
}

// Convert angle (0-180°) to hardware PWM pulse width and send to PCA9685
void writeServoPulse(int channel, float angle) {
  angle = constrain(angle, 0, 180);
  int pulse = map((int)angle, 0, 180, SERVOMIN, SERVOMAX);
  pwm.setPWM(channel, 0, pulse);
}

// Control continuous rotation base motor speed and direction (CH0)
// Continuous rotation base: percent is -100 (full CCW) .. +100 (full CW).
// Symmetric about the servo's real neutral, so CW and CCW mirror each other.
void setBaseSpeed(int percent) {
  percent = constrain(percent, -100, 100);
  if (percent == 0) {
    // Stop = no pulse at all. Any pulse near neutral still makes the base creep,
    // so cutting the signal is the only reliable way to hold it still.
    pwm.setPWM(BASE_SERVO, 0, 0);
    return;
  }
  int pulse = BASE_PULSE_STOP + (long)BASE_PULSE_SPAN * percent / 100;
  pwm.setPWM(BASE_SERVO, 0, pulse);
}