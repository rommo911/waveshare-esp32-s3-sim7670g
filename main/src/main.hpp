#ifndef MAIN_H_
#define MAIN_H_
#include <Arduino.h>

void turnOnCamera();
void turnOffCamera();
bool getCamIsON();

uint64_t getLastCamOnTs();

uint64_t getLastCamOffTs();

void loadTimingPref();

uint32_t getWifiTimeout();

uint32_t getNoMotionTimeout();

uint32_t getSecureModeTimeout();

uint64_t getSnapShotTime();

void checkOTA_rollback(void *arg);

extern uint32_t OTA_VALIDATION_COUNTER;

void light_sleep(uint16_t seconds);
#endif // MAIN_H_