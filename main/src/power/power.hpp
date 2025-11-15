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

namespace power
{
    enum WakeUpReason : uint8_t
    {
        MOTION = 0,
        START = 1,
        TIMER = 2,
        UNKNOWN
    };

    enum TimerSleepCause_t : uint8_t
    {
        AFTER_MOTION = 0,
        SLEEP_SNAP_SHOT = 2,
        AFTER_ON = 4,
        NA
    };
    bool setupPower();
    esp_sleep_wakeup_cause_t getWakeupReason();
    WakeUpReason Get_wake_reason();
    TimerSleepCause_t getSleepCause();
    static inline const char *GetTimerSleepCause(TimerSleepCause_t cause)
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
    void DeepSleepWith_IMU_Timer_Wake(TimerSleepCause_t sleepcause = NA, uint32_t ms = 0);
    void DeepSleepWith_PMU_Timer_Wake(TimerSleepCause_t sleepcause = NA, uint32_t ms = 0);
    void DeepSleepWith_PMU_Wake();

};

// helper: map 0..100% to a red->yellow->green gradient
