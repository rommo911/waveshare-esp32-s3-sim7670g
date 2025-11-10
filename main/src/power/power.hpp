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
    bool setupPower();
    esp_sleep_wakeup_cause_t getWakeupReason();
    WakeUpReason Get_wake_reason();

    Adafruit_MAX17048 &getPMU();
    bool isBattCharging();
    bool isPowerVBUSOn();
    bool isBatLowLevel();
    bool isBatCriticalLevel();
    void DeepSleepWith_IMU_PMU_Wake();
    void DeepSleepWith_PMU_Wake();
    uint64_t getLastVbusInsertedTs();
    uint64_t getLastVbusRemovedTs();
    void DeepSleepWith_IMU_Timer_Wake(uint32_t ms);
    void DeepSleepWith_Timer_Wake(uint32_t ms);
};

// helper: map 0..100% to a red->yellow->green gradient
