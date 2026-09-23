#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <WiFi.h>

void handleWebRequest(WiFiClient& client, String request);
int getParameter(String request, String parameter);
String getStringParameter(String request, String parameter);
void sendResponse(WiFiClient& client, String message);
void sendHomePage(WiFiClient& client);

#endif