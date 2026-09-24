#ifndef SERVOS_H
#define SERVOS_H

#include <Adafruit_PWMServoDriver.h>

extern Adafruit_PWMServoDriver pwm;
extern float currentAngle[];
extern int targetAngle[];

void initServos();
void homeServos();   // ease all joints to HOME_POSE and stop the base
void stepServos();
void writeServoPulse(int channel, float angle);
void setBaseSpeed(int percent);   // -100..+100, 0 = stop (CH0 only)

// Neutral pulse of the continuous-rotation base, in PCA9685 counts.
// Raise it to make CW faster and CCW slower; lower it for the opposite.
// Tunable at runtime so the two directions can be balanced without reflashing.
void setBaseTrim(int pulseStop);
int  getBaseTrim();

#endif