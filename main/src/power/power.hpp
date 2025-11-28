/**
 * @file      power.h
 * @author    Lewis He (lewishe@outlook.com)
 * @license   MIT
 * @copyright Copyright (c) 2022  Shenzhen Xin Yuan Electronic Technology Co., Ltd
 * @date      2022-09-16
 *
 */
#pragma once

#include "Adafruit_MAX1704X.h"
#include "pins.hpp"
namespace power
{
    enum WakeUpReason_t : uint8_t
    {
        MOTION = 0,
        START = 1,
        TIMER = 2,
        MOEDM = 3,
        UNKNOWN
    };

    enum WakeUpPin_t : int32_t
    {
        MOTION_PIN = MOTION_INTRRUPT_PIN,
        START_PIN = VBUS_INPUT_PIN,
    };

    enum TimerWakeReason_t : uint8_t
    {
        AFTER_MOTION = 0,
        SLEEP_SNAP_SHOT = 2,
        AFTER_ON = 4,
        Battery_LOW = 5,
        WORKER = 6,
        NA
    };
    bool setupPower();
    esp_sleep_wakeup_cause_t getWakeupReason();
    WakeUpReason_t Get_wake_reason();
    TimerWakeReason_t getTimerWakeReason();
    static inline const char *GetTimerSleepCause(TimerWakeReason_t cause)
    {
        switch (cause)
        {
        case AFTER_MOTION:
            return "AFTER_MOTION";
        case SLEEP_SNAP_SHOT:
            return "SLEEP_SNAP_SHOT";
        default:
            return "NA";
        }
    }

    Adafruit_MAX17048 &getPMU();
    bool iskeyShortPressed();
    float getCellPercent();
    float getCellVoltage();
    bool isPowerVBUSOn();
    bool isBatLowLevel();
    bool isBatCriticalLevel();
    uint64_t getLastVbusInsertedTs();
    uint64_t getLastVbusRemovedTs();
    void Sleep_EnableTimer(TimerWakeReason_t cause, uint32_t ms);
    void Sleep_DisableTimer();
    void Sleep_EnablePinWakeup(WakeUpPin_t pin);
    void Sleep_DisablePinWakeup();
    void Sleep_DisableAllWakeup();
    void DeepSleep();
    void light_sleep(uint16_t seconds);
};

// helper: map 0..100% to a red->yellow->green gradient
