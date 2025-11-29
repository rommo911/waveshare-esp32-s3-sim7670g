#pragma once 
#include <MqttLogger.h>

extern MqttLogger mqttLogger;
extern PubSubClient mqttclient;

void StartWifi();
bool GetWifiOn();
void StopWifi();
bool syncntpTime();