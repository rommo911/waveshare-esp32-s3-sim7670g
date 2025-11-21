
/**
 * @file      AllFunction.ino
 * @author    Lewis He (lewishe@outlook.com)
 * @license   MIT
 * @copyright Copyright (c) 2022  Shenzhen Xin Yuan Electronic Technology Co., Ltd
 * @date      2022-09-16
 *
 */
#include <Arduino.h>
#include "sdcard/sdcard.h"
#include "wifi/wifi.hpp"
#include "power/power.hpp"
#include "pins.hpp"
#include "main.hpp"
#include "led/led.hpp"
#include "imu6500/imu_DMP6.hpp"
#include "wifi/wifi.hpp"
#include "Preferences.h"
#include "nvs_flash.h"
#include <thread>
#include "esp_ota_ops.h"
#include "pppos_client.h"
#include "system.hpp"

bool simulatedMotionTrigger = false;
bool simulatedLowPowerTrigger = false;
bool simulatedCriticalLowPowerTrigger = false;
bool ota_needValidation = false;
uint64_t LastWifiOnTimestamp = 0;
uint32_t RTC_DATA_ATTR motionCounter;
uint32_t OTA_VALIDATION_COUNTER = 0;

static inline bool NoMotionSince(const uint32_t timeout)
{
    return (millis() - imu6500_dmp::getLastMovedTimestamp() > timeout);
}

static inline bool NoVbusSince(const uint32_t timeout)
{
    return (millis() - power::getLastVbusRemovedTs() > timeout);
}

void CheckMotionCount()
{
    if (motionCounter > 0)
    {
        motionCounter = 0;
        led::start_blink(0, led::_rgb(50, 50, 50), led::_rgb(255, 0, 0), 250, 150, 5000);
        delay(5000);
    }
}

void setup()
{
    uint8_t counter = 0;
    Serial.begin(115200);
    power::WakeUpReason wu = power::Get_wake_reason();
    power::setupPower();
    ota_needValidation = check_rollback();
    setCpuFrequencyMhz(160);
    StartModem();
    while (1)
    {
        light_sleep(1);
    }
    // Serial.setTxBufferSize(512);
    led::led_init();
    led::set_solid(0, led::_rgb(0, 0, 30));
    turnOnCamera();
    loadTimingPref();
    if (imu6500_dmp::imu_setup(imu6500_dmp::WOM))
    {
        Serial.println("IMU setup complete ");
    }
    else
    {
        Serial.println("IMU setup failed ");
        led::start_blink(0, led::_rgb(255, 0, 0));
        delay(5000);
        led::stop_led(0);
        delay(50);
        /// ESP.restart();
    }
    if (power::isPowerVBUSOn())
    {
        StartWifi();
        LastWifiOnTimestamp = millis();
    }
    else
    {
        switch (wu)
        {
        case power::WakeUpReason::UNKNOWN:
        {
            mqttLogger.println("wake UNKNOWN...");
            motionCounter = 0;
            break;
        }
        case power::WakeUpReason::START:
        {
            mqttLogger.println("wake FROM START...");
            break;
        }
        case power::WakeUpReason::MOTION:
        {
            mqttLogger.println("wake FROM MOTION");
            led::start_blink(0, led::_rgb(0, 0, 100), 0, 200, 200, 2000);
            delay(1500);
            if (power::isBatLowLevel())
            {
                led::set_solid(0, led::_rgb(0, 5, 5));
                // led::set_solid(1, led::_rgb(5, 0, 0));
            }
            else
            {
                led::set_solid(0, led::_rgb(0, 20, 20));
                // led::set_solid(1, led::_rgb(20, 0, 0));
            }
            if (!power::isPowerVBUSOn())
            {
                if (power::isBatLowLevel())
                {
                    power::DeepSleepWith_IMU_Timer_Wake(power::AFTER_MOTION, getNoMotionTimeout()); // dont keep waking up for new motion !
                }
                else
                {
                    power::DeepSleepWith_IMU_Timer_Wake(power::AFTER_MOTION, getNoMotionTimeout()); // wake up and reset timer if new motion is detected before expires
                }
            }

            break;
        }
        case power::WakeUpReason::TIMER:
        {
            mqttLogger.println("wake FROM TIMER");
            if (!imu6500_dmp::getMotion() && !power::isPowerVBUSOn())
            {
                if (power::getSleepCause() == power::SLEEP_SNAP_SHOT)
                {
                    led::start_fade(0, led::_rgb(50, 50, 50), 0, 500, 4);
                    mqttLogger.println("wake FROM Timer for snapshot...");
                    delay(5000);
                }
                else
                {
                    led::start_blink(0, led::_rgb(50, 50, 50), 0, 200, 200, 500);
                    mqttLogger.println("wake FROM Timer after no motion .. turn off Cam");
                    delay(500);
                    motionCounter++;
                }
                turnOffCamera();
                if (power::isBatLowLevel())
                    led::set_solid(0, led::_rgb(2, 0, 0)); // turn off led before sleep
                else
                    led::set_solid(0, led::_rgb(0, 0, 2)); // turn off led before sleep

                power::DeepSleepWith_IMU_Timer_Wake(power::SLEEP_SNAP_SHOT, getSnapShotTime());
            }

            break;
        }
        default:
        {
            mqttLogger.println("wake FROM unknwon");
            break;
        }
        }
    }

    if (/*power::isPowerVBUSOn()*/ true && ota_needValidation == false)
    {
        if (sdcard::checkForupdatefromSD())
        {
            Serial.println("update done restarting");
            delay(100);
            ESP.restart();
        }
    }
}

void loopPowerCheck()
{
    if (power::isPowerVBUSOn())
    {
        const uint8_t percent = power::getCellPercent();
        led::set_solid(0, led::batteryColor(percent));
        CheckMotionCount();
        return;
    }
    if (power::isBatCriticalLevel())
    {
        mqttLogger.printf("Battery critical level detected in main loop \n");
        led::start_blink(0, led::_rgb(20, 0, 0), 0, 75, 125, 500);
        delay(500);
        led::stop_led(0);
        power::DeepSleepWith_PMU_Wake();
    }
    if (power::isBatLowLevel() || simulatedLowPowerTrigger)
    {
        simulatedLowPowerTrigger = false;
        mqttLogger.printf("Battery low level detected in main loop \n");
        led::set_solid(0, led::_rgb(2, 0, 0)); // turn off led before sleep
        delay(30);
        power::DeepSleepWith_IMU_Timer_Wake(power::AFTER_ON);
    }
}

bool waitForCarhelper = false;
void loopImuMotion()
{
    if (imu6500_dmp::getMotion())
    {
        if (!waitForCarhelper)
            led::start_blink(0, led::_rgb(0, 0, 100), 0, 150, 200, 150); // turn off led before sleep
        delay(150);
    }
    if (power::isPowerVBUSOn())
    {
        waitForCarhelper = true;
        return;
    }
    // wait untill no motion for a while and Vbus removed for a while
    if (NoMotionSince(getNoMotionTimeout()) && NoVbusSince(getSecureModeTimeout()) && !GetWifiOn())
    {
        mqttLogger.println("starting secure mode");
        turnOffCamera();
        led::set_solid(0, led::_rgb(0, 0, 2)); //
        power::DeepSleepWith_IMU_Timer_Wake(power::SLEEP_SNAP_SHOT, getSnapShotTime());
    }
    else
    {
        if (waitForCarhelper) // only once
        {
            led::start_blink(0, led::batteryColor(power::getCellPercent()), 0, 100, 2500, 120 * 1000); // crete blink patter to inform user its waiting
            waitForCarhelper = false;
            mqttLogger.println("waiting for some time after vbus removed");
        }
    }
    return;
}

void loopWifiStatus()
{
    if (power::iskeyShortPressed())
    {
        mqttLogger.println("Power key short pressed detected in main loop");
        if (!GetWifiOn())
        {
            // led::start_blink(0, led::_rgb(0, 50, 0), 0, 200, 2500, 120 * 1000); // crete blink patter to inform user its waiting
            StartWifi();
            LastWifiOnTimestamp = millis();
        }
    }
    if (GetWifiOn())
    {
        if (((millis() - LastWifiOnTimestamp > getWifiTimeout()) || (!power::isPowerVBUSOn() && power::isBatLowLevel())))
        {
            mqttLogger.println("WiFi on timeout reached, turning off WiFi");
            StopWifi();
        }
    }
}

void loop()
{
    loopWifiStatus();
    loopPowerCheck();
    loopImuMotion();
    if (ota_needValidation && OTA_VALIDATION_COUNTER++ > 2000)
    {
        ota_needValidation = false;
        set_ota_valid(true);
    }
    delay(10);
}

extern "C" void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, NULL);
        if (partition != NULL)
        {
            err = esp_partition_erase_range(partition, 0, partition->size);
            if (!err)
            {
                err = nvs_flash_init();
            }
            else
            {
                log_e("Failed to format the broken NVS partition!");
            }
        }
    }
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_app_desc_t running_app_info;
    if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK)
    {
        ESP_LOGI("MAIN", "Running firmware version: %s", running_app_info.version);
    }
    setup();
    while (1)
    {
        loop();
    }
}