#ifndef MQTT_CONTROL_H
#define MQTT_CONTROL_H

// Connects to Wi-Fi + MQTT broker and translates incoming messages
// into servo commands. All functions are non-blocking so stepServos()
// keeps running smoothly while (re)connecting.

void mqttSetup();   // call once in setup()
void mqttLoop();    // call every loop(): keeps connection alive, publishes state

#endif
