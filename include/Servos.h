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

#endif