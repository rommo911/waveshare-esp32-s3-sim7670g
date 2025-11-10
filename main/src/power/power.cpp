/**
 * @file      power.cpp
 * @author    Lewis He (lewishe@outlook.com)
 * @license   MIT
 * @copyright Copyright (c) 2022  Shenzhen Xin Yuan Electronic Technology Co., Ltd
 * @date      2022-09-16
 *
 */

#include <Arduino.h>
#include "pins.hpp"
#include "wifi/wifi.hpp"
#include "power/power.hpp"
#include "driver/rtc_io.h"

#include <atomic>

namespace power
{
    float cellVoltage = 0.0f;
    esp_sleep_wakeup_cause_t wakeup_reason;
    float rate = 0.0f;
    std::atomic<bool> isCharging;
    std::atomic<bool> isBatteryLowLevel;
    std::atomic<bool> isBatteryCriticalLevel;
    std::atomic<bool> isPekeyShortPressed;

    uint64_t VbusInsertTimestamp = 0;
    uint64_t VbusRemovedTimestamp = 0;

    void loopPower(void *arg);

    Adafruit_MAX17048 PMU;

    EventGroupHandle_t pmuIrqEvent;

    void IRAM_ATTR setFlag(void)
    {
        xEventGroupSetBits(pmuIrqEvent, 0b1);
    }

    bool setupPower()
    {
        isCharging = false;
        isBatteryLowLevel = false;
        isBatteryCriticalLevel = false;
        isPekeyShortPressed = false;
        Wire.begin(I2C_SDA_POWER, I2C_SCL_POWER, 400000);

        if (!PMU.begin(&Wire))
        {
            mqttLogger.println("ERROR: Init PMU failed!");
            return false;
        }
        Serial.print(F("Found MAX17048"));
        Serial.print(F(" with Chip ID: 0x"));
        Serial.println(PMU.getChipID(), HEX);
        Serial.print(F("Reset voltage = "));
        Serial.print(PMU.getResetVoltage());
        Serial.println(" V");
        Serial.print(F("Activity threshold = "));
        Serial.print(PMU.getActivityThreshold());
        Serial.println(" V change");

        Serial.print(F("Hibernation threshold = "));
        Serial.print(PMU.getHibernationThreshold());
        Serial.println(" %/hour");

        // The alert pin can be used to detect when the voltage of the battery goes below or
        // above a voltage, you can also query the alert in the loop.
        PMU.setAlertVoltages(3.2, 4.3);
        float alert_min, alert_max;
        PMU.getAlertVoltages(alert_min, alert_max);
        Serial.print("Alert voltages: ");
        Serial.print(alert_min);
        Serial.print(" ~ ");
        Serial.print(alert_max);
        Serial.println(" V");
        cellVoltage = PMU.cellVoltage();
        if (isnan(cellVoltage))
        {
            Serial.println("Failed to read cell voltage, check battery is connected!");
        }
        isBatteryCriticalLevel = PMU.cellPercent() <= 5;
        isBatteryLowLevel = PMU.cellPercent() <= 15;

        Serial.print(F("Batt Voltage: "));
        Serial.print(cellVoltage, 3);
        Serial.println(" V");
        Serial.print(F("Batt Percent: "));
        Serial.print(PMU.cellPercent(), 1);
        Serial.println(" %");
        Serial.print(F("(Dis)Charge rate : "));
        Serial.print(PMU.chargeRate(), 1);
        Serial.println(" %/hr");
        if (PMU.isHibernating())
        {
            Serial.println(F("Hibernating!"));
        }
        xTaskCreate(loopPower, "power", 4096, NULL, 1, NULL);
        return true;
    }

    void loopPower(void *arg)
    {
        while (1)
        {
            delay(10000);
            cellVoltage = PMU.cellVoltage();
            rate = PMU.chargeRate();
            if (isnan(cellVoltage))
            {
                Serial.println("Failed to read cell voltage, check battery is connected!");
                continue;
            }
            else
            {
                Serial.printf(" cell %.2f v , rate %.2f %%\n", cellVoltage, rate);
                isCharging = rate > 0;
                isBatteryCriticalLevel = PMU.cellPercent() <= 5;
                isBatteryLowLevel = PMU.cellPercent() <= 15;
            }
            if (PMU.isActiveAlert())
            {
                uint8_t status_flags = PMU.getAlertStatus();
                Serial.print(F("ALERT! flags = 0x"));
                Serial.print(status_flags, HEX);
                if (status_flags & MAX1704X_ALERTFLAG_SOC_CHANGE)
                {
                    Serial.print(", SOC Change");
                    PMU.clearAlertFlag(MAX1704X_ALERTFLAG_SOC_CHANGE); // clear the alert
                }
                if (status_flags & MAX1704X_ALERTFLAG_SOC_LOW)
                {
                    Serial.print(", SOC Low");
                    PMU.clearAlertFlag(MAX1704X_ALERTFLAG_SOC_LOW); // clear the alert
                }
                if (status_flags & MAX1704X_ALERTFLAG_VOLTAGE_RESET)
                {
                    Serial.print(", Voltage reset");
                    PMU.clearAlertFlag(MAX1704X_ALERTFLAG_VOLTAGE_RESET); // clear the alert
                }
                if (status_flags & MAX1704X_ALERTFLAG_VOLTAGE_LOW)
                {
                    Serial.print(", Voltage low ");
                    Serial.print(cellVoltage, 3);
                    PMU.clearAlertFlag(MAX1704X_ALERTFLAG_VOLTAGE_LOW); // clear the alert
                }
                if (status_flags & MAX1704X_ALERTFLAG_VOLTAGE_HIGH)
                {
                    Serial.print(", Voltage high ");
                    Serial.print(cellVoltage, 3);
                    PMU.clearAlertFlag(MAX1704X_ALERTFLAG_VOLTAGE_HIGH); // clear the alert
                }
                if (status_flags & MAX1704X_ALERTFLAG_RESET_INDICATOR)
                {
                    Serial.print(", Reset Indicator");
                    PMU.clearAlertFlag(MAX1704X_ALERTFLAG_RESET_INDICATOR); // clear the alert
                }
                Serial.println();
            }
        }
    }

    esp_sleep_wakeup_cause_t getWakeupReason()
    {
        wakeup_reason = esp_sleep_get_wakeup_cause();

        switch (wakeup_reason)
        {
        case ESP_SLEEP_WAKEUP_UNDEFINED:
            //!< In case of deep sleep, reset was not caused by exit from deep sleep
            mqttLogger.println("In case of deep sleep, reset was not caused by exit from deep sleep");
            break;
        case ESP_SLEEP_WAKEUP_ALL:
            //!< Not a wakeup cause: used to disable all wakeup sources with esp_sleep_disable_wakeup_source
            mqttLogger.println("Not a wakeup cause: used to disable all wakeup sources with esp_sleep_disable_wakeup_source");
            break;
        case ESP_SLEEP_WAKEUP_EXT0:
            //!< Wakeup caused by external signal using RTC_IO
            mqttLogger.println("Wakeup caused by external signal using RTC_IO");
            break;
        case ESP_SLEEP_WAKEUP_EXT1:
            //!< Wakeup caused by external signal using RTC_CNTL
            mqttLogger.println("Wakeup caused by external signal using RTC_CNTL");
            break;
        case ESP_SLEEP_WAKEUP_TIMER:
            //!< Wakeup caused by timer
            mqttLogger.println("Wakeup caused by timer");
            break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            //!< Wakeup caused by touchpad
            mqttLogger.println("Wakeup caused by touchpad");
            break;
        case ESP_SLEEP_WAKEUP_ULP:
            //!< Wakeup caused by ULP program
            mqttLogger.println("Wakeup caused by ULP program");
            break;
        case ESP_SLEEP_WAKEUP_GPIO:
            //!< Wakeup caused by GPIO (light sleep only)
            mqttLogger.println("Wakeup caused by GPIO (light sleep only)");
            break;
        case ESP_SLEEP_WAKEUP_UART:
            //!< Wakeup caused by UART (light sleep only)
            mqttLogger.println("Wakeup caused by UART (light sleep only)");
            break;
        case ESP_SLEEP_WAKEUP_WIFI:
            //!< Wakeup caused by WIFI (light sleep only)
            mqttLogger.println("Wakeup caused by WIFI (light sleep only)");
            break;
        case ESP_SLEEP_WAKEUP_COCPU:
            //!< Wakeup caused by COCPU int
            mqttLogger.println("Wakeup caused by COCPU int");
            break;
        case ESP_SLEEP_WAKEUP_COCPU_TRAP_TRIG:
            //!< Wakeup caused by COCPU crash
            mqttLogger.println("Wakeup caused by COCPU crash");
            break;
        case ESP_SLEEP_WAKEUP_BT:
            //!< Wakeup caused by BT (light sleep only)
            mqttLogger.println("Wakeup caused by BT (light sleep only)");
            break;
        default:
            mqttLogger.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
            break;
        }
        return wakeup_reason;
    }

    Adafruit_MAX17048 &getPMU()
    {
        return PMU;
    }
    bool isPowerVBUSOn()
    {
        return true;
    }

    bool isBattCharging()
    {
        return isCharging;
    }

    uint64_t getLastVbusInsertedTs()
    {
        return 0;
    }
    uint64_t getLastVbusRemovedTs()
    {
        return 0;
    }
    bool isBatLowLevel()
    {
        return isBatteryLowLevel;
    }

    bool isBatCriticalLevel()
    {
        return isBatteryCriticalLevel;
    }

    void DeepSleepWith_IMU_PMU_Wake()
    {
        // Configure wakeup source: IMU interrupt pin
        mqttLogger.println("Entering deep sleep mode with IMU and PMU wakeup");
        detachInterrupt(MOTION_INTRRUPT_PIN);
        rtc_gpio_hold_en(MOTION_INTRRUPT_PIN);
        rtc_gpio_hold_en(VBUS_PMU_INPUT_PIN);
        rtc_gpio_hold_en(CAM_PIN);
        uint64_t wakeup_mask = (1ULL << MOTION_INTRRUPT_PIN) | (1ULL << VBUS_PMU_INPUT_PIN);
        String str = "Going to sleep now with mask  " + String(wakeup_mask, BIN);
        mqttLogger.printf(str.c_str());
        ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup_io(wakeup_mask, ESP_EXT1_WAKEUP_ANY_LOW));
        delay(100);
        esp_deep_sleep_start();
    }

    void DeepSleepWith_PMU_Wake()
    {
        mqttLogger.println("Entering deep sleep mode with PMU wakeup");
        // Configure wakeup source: IMU interrupt pin
        detachInterrupt(VBUS_PMU_INPUT_PIN);
        detachInterrupt(MOTION_INTRRUPT_PIN);
        rtc_gpio_hold_en(CAM_PIN);
        uint64_t wakeup_mask = (1ULL << VBUS_PMU_INPUT_PIN);
        String str = "Going to sleep now with mask  " + String(wakeup_mask, BIN);
        mqttLogger.printf(str.c_str());
        ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup_io(wakeup_mask, ESP_EXT1_WAKEUP_ANY_HIGH));
        delay(100);
        esp_deep_sleep_start();
    }

    void DeepSleepWith_IMU_Timer_Wake(uint32_t ms)
    {
        mqttLogger.println("Entering deep sleep mode with timer PMU wakeup");
        // Configure wakeup source: IMU interrupt pin
        detachInterrupt(VBUS_PMU_INPUT_PIN);
        detachInterrupt(MOTION_INTRRUPT_PIN);
        rtc_gpio_hold_en(MOTION_INTRRUPT_PIN);
        rtc_gpio_hold_en(VBUS_PMU_INPUT_PIN);
        rtc_gpio_hold_en(CAM_PIN);
        uint64_t wakeup_mask = (1ULL << MOTION_INTRRUPT_PIN) | (1ULL << VBUS_PMU_INPUT_PIN);
        String str = "Going to sleep now with mask " + String(wakeup_mask, BIN) + String(ms);
        mqttLogger.printf(str.c_str());
        esp_sleep_enable_ext1_wakeup_io(wakeup_mask, ESP_EXT1_WAKEUP_ANY_HIGH);
        esp_sleep_enable_timer_wakeup(1000 * ms);
        esp_deep_sleep_start();
    }

    void DeepSleepWith_Timer_Wake(uint32_t ms)
    {
        mqttLogger.println("Entering deep sleep mode with timer PMU wakeup");
        // Configure wakeup source: IMU interrupt pin
        detachInterrupt(VBUS_PMU_INPUT_PIN);
        detachInterrupt(MOTION_INTRRUPT_PIN);
        rtc_gpio_hold_en(MOTION_INTRRUPT_PIN);
        rtc_gpio_hold_en(VBUS_PMU_INPUT_PIN);
        rtc_gpio_hold_en(CAM_PIN);
        uint64_t wakeup_mask = (1ULL << VBUS_PMU_INPUT_PIN);
        String str = "Going to sleep now with mask " + String(wakeup_mask, BIN) + String(ms);
        mqttLogger.printf(str.c_str());
        esp_sleep_enable_ext1_wakeup_io(wakeup_mask, ESP_EXT1_WAKEUP_ANY_HIGH);
        esp_sleep_enable_timer_wakeup(1000 * ms);
        esp_deep_sleep_start();
    }

    WakeUpReason Get_wake_reason()
    {
        static WakeUpReason wakeUpReason = WakeUpReason::UNKNOWN;
        if (wakeUpReason != WakeUpReason::UNKNOWN)
        {
            return wakeUpReason;
        }
        auto cause = esp_sleep_get_wakeup_cause();
        switch (cause)
        {
        case ESP_SLEEP_WAKEUP_EXT1:
        {
            uint64_t wakeup_pin_mask = esp_sleep_get_ext1_wakeup_status();
            if (wakeup_pin_mask & ((uint64_t)1 << MOTION_INTRRUPT_PIN))
            {
                Serial.println("Wakeup cause detected: MPU motion interrupt");
                wakeUpReason = WakeUpReason::MOTION;
                break;
            }
            if (wakeup_pin_mask & ((uint64_t)1 << VBUS_PMU_INPUT_PIN))
            {
                Serial.println("Wakeup cause detected: PMU interrupt");
                wakeUpReason = WakeUpReason::START;
                break;
            }
            else
            {
                Serial.printf("Wakeup cause detected: 0x%llx\n", wakeup_pin_mask);
                break;
            }
        }
        case ESP_SLEEP_WAKEUP_TIMER:
        {
            wakeUpReason = WakeUpReason::TIMER;
            Serial.println("Wakeup cause detected: TIMER");
            break;
        }
        default:
        {
            wakeUpReason = WakeUpReason::UNKNOWN;
            break;
        }
        }
        return wakeUpReason;
    }
  
};
