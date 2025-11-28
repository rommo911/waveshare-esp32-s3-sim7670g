
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
#include "pppos_client.hpp"
#include "system.hpp"
#include "CarBattery/CarBattery.hpp"

bool simulatedMotionTrigger = false;
bool simulatedLowPowerTrigger = false;
bool simulatedCriticalLowPowerTrigger = false;
bool ota_needValidation = false;
uint64_t LastWifiOnTimestamp = 0;
uint32_t RTC_DATA_ATTR motionCounter;
uint32_t RTC_DATA_ATTR motionOngoing;
uint32_t OTA_VALIDATION_COUNTER = 0;
CarBattery carBattery;

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
        builtinLed.setBlink(0, LedStrip::_rgb(50, 50, 50), LedStrip ::_rgb(255, 0, 0), 250, 150, 10);
        delay(5000);
    }
}

void handleWakeup()
{
    power::WakeUpReason_t wu = power::Get_wake_reason();
    switch (wu)
    {
    case power::WakeUpReason_t::UNKNOWN:
    {
        mqttLogger.println("wake UNKNOWN...");
        motionCounter = 0;
        break;
    }
    case power::WakeUpReason_t::START:
    {
        mqttLogger.println("wake FROM START...");
        break;
    }
    case power::WakeUpReason_t::MOTION:
    {
        mqttLogger.println("wake FROM MOTION");
        builtinLed.setBlink(0, LedStrip::_rgb(0, 0, 100), 0, 200, 200, 0, 1000, LedStrip::Priority::NOTIFICATION);
        delay(1000);
        if (power::isBatLowLevel())
        {
            builtinLed.setSolid(0, LedStrip::_rgb(0, 5, 5));
            // led::set_solid(1, led::_rgb(5, 0, 0));
        }
        else
        {
            builtinLed.setSolid(0, LedStrip::_rgb(0, 20, 20));
            // led::set_solid(1, led::_rgb(20, 0, 0));
        }
        if (!power::isPowerVBUSOn())
        {
            // power::Sleep_EnablePinWakeup(power::WakeUpPin_t::START_PIN);
            if (power::isBatLowLevel())
            {
                power::DeepSleep(); // dont keep waking up for new motion !
            }
            else
            {
                if (motionOngoing == false)
                {
                    motionOngoing = true;
                    modem.init();
                    modem.sendSMS("0758829590", "Motion detected!");
                    modem.sleep(ModemSim7670::SleepMode_t::INTERRUPT_READY);
                }
                power::Sleep_EnableTimer(power::AFTER_MOTION, getNoMotionTimeout());
                power::Sleep_EnablePinWakeup(power::WakeUpPin_t::MOTION_PIN);
                power::DeepSleep(); // wake up and reset timer if new motion is detected before expires
            }
        }

        break;
    }
    case power::WakeUpReason_t::TIMER:
    {
        mqttLogger.println("wake FROM TIMER");
        if (!imu6500_dmp::getMotion() && !power::isPowerVBUSOn())
        {
            switch (power::getTimerWakeReason())
            {
            case power::SLEEP_SNAP_SHOT:
            {
                builtinLed.setFade(0, LedStrip::_rgb(50, 50, 50), 0, 500, 0, 5000);
                mqttLogger.println("wake FROM Timer for snapshot...");
                delay(5000);
                break;
            }
            case power::AFTER_MOTION:
            {
                motionCounter++;
                motionOngoing = false;
                builtinLed.setFade(0, LedStrip::_rgb(50, 50, 50), 0, 500, 0, 5000);
                mqttLogger.println("wake FROM Timer after no motion .. turn off Cam");
                delay(500);
                if (power::isBatLowLevel() == false)
                {
                    // here try wakeup modem and send SMS
                    modem.init();
                    auto sms = std::string("motion detection done, counter=") + std::to_string(motionCounter) + std::string(". sleeping.");
                    modem.sendSMS("0758829590", sms.c_str());
                    modem.sleep(ModemSim7670::SleepMode_t::SLEEP);
                }
                break;
            }
            default:
            {
                break;
            }
            }
            turnOffCamera();
            if (power::isBatLowLevel())
                builtinLed.setSolid(0, LedStrip::_rgb(2, 0, 0)); // turn off led before sleep
            else
                builtinLed.setSolid(0, LedStrip::_rgb(0, 0, 2)); // turn off led before sleep
            power::Sleep_EnableTimer(power::SLEEP_SNAP_SHOT, getSnapShotTime());
            power::Sleep_EnablePinWakeup(power::WakeUpPin_t::MOTION_PIN);
            // power::Sleep_EnablePinWakeup(power::WakeUpPin_t::START_PIN);
            power::DeepSleep();
        }
        break;
    }
    case power::WakeUpReason_t::MOEDM:
    {
        if (power::isBatLowLevel() == false)
        {
            // here try wakeup modem and send SMS
            modem.init();
            std::list<sms_t> sms_list;
            modem.GetDce()->get_unread_sms_list(sms_list);
            if (sms_list.size() > 0)
            {
                for (auto &sms : sms_list)
                {
                    ESP_LOGI("sms", " sms index %d ts : %d received from %s , content %s  ", sms.index, sms.timestamp, sms.sender.c_str(), sms.content.c_str());
                    if (sms.sender == "0758829590")
                    {
                        std::string response = " respond wake up after got " + sms.content;
                        modem.sendSMS("0758829590", response.c_str());
                        modem.EnableGnss(true);
                        power::light_sleep(30);
                        sim76xx_gps_t gps;
                        bool ret = modem.get_gnss(gps, 60);
                        if (ret)
                        {
                            response = gps.pretty_string();
                            modem.sendSMS("0758829590", response.c_str());
                        }
                        else
                        {
                            modem.sendSMS("0758829590", "no gps fix");
                        }
                    }
                    modem.GetDce()->delete_sms(sms.index);
                }
            }
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

void setup()
{
    // uint8_t counter = 0;
    Serial.begin(115200);
    power::setupPower();
    builtinLed.begin();
    NotifyLed.begin();
    carBattery.begin();
    ota_needValidation = check_rollback();
    delay(1500);
    setCpuFrequencyMhz(80);
    builtinLed.setSolid(0, LedStrip::_rgb(0, 0, 25), LedStrip::Priority::NORMAL);
    NotifyLed.setSolid(0, LedStrip::_rgb(0, 0, 10), LedStrip::Priority::NORMAL);
    NotifyLed.setSolid(1, LedStrip::_rgb(0, 0, 5), LedStrip::Priority::NORMAL);
    turnOnCamera();
    loadTimingPref();
    if (imu6500_dmp::imu_setup(imu6500_dmp::WOM))
    {
        Serial.println("IMU setup complete ");
    }
    else
    {
        Serial.println("IMU setup failed ");
        builtinLed.setBlink(0, LedStrip::_rgb(255, 0, 0), 0, 200, 200, 0, 5000, LedStrip::Priority::ALERT);
        delay(5000);
        builtinLed.clear(0);
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
        handleWakeup();
    }
    if (power::isPowerVBUSOn() && ota_needValidation == false)
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
        builtinLed.setSolid(0, LedStrip::batteryColor(percent));
        CheckMotionCount();
        return;
    }
    if (power::isBatCriticalLevel())
    {
        mqttLogger.printf("Battery critical level detected in main loop \n");
        builtinLed.setBlink(0, LedStrip::_rgb(20, 0, 0), 0, 75, 125, 500);
        delay(500);
        builtinLed.clear(0);
        power::Sleep_DisableTimer();
        power::Sleep_EnablePinWakeup(power::WakeUpPin_t::START_PIN);
        power::DeepSleep();
    }
    if (power::isBatLowLevel())
    {
        mqttLogger.printf("Battery low level detected in main loop \n");
        builtinLed.setSolid(0, LedStrip::_rgb(2, 0, 0)); // turn off led before sleep
        delay(30);
        // power::Sleep_EnablePinWakeup(power::WakeUpPin_t::START_PIN);
        power::Sleep_EnablePinWakeup(power::WakeUpPin_t::MOTION_PIN);
        power::DeepSleep();
    }
}

bool waitForCarhelper = false;
void loopImuMotion()
{
    if (imu6500_dmp::getMotion())
    {
        // if (!waitForCarhelper)
        builtinLed.setBlink(0, LedStrip::_rgb(0, 0, 100), 0, 100, 150, 1, 0, LedStrip::Priority::NOTIFICATION); // turn off led before sleep
        delay(150);
        Serial.println("motion detected");
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
        builtinLed.setSolid(0, LedStrip::_rgb(0, 0, 2)); //
        power::Sleep_EnableTimer(power::SLEEP_SNAP_SHOT, getSnapShotTime());
        // power::Sleep_EnablePinWakeup(power::WakeUpPin_t::START_PIN);
        power::Sleep_EnablePinWakeup(power::WakeUpPin_t::MOTION_PIN);
        power::DeepSleep();
    }
    else
    {
        if (waitForCarhelper) // only once
        {
            builtinLed.setBlink(0, LedStrip::batteryColor(power::getCellPercent()), 0, 100, 2500, 0, 120U * 1000U, LedStrip::Priority::NOTIFICATION); // crete blink patter to inform user its waiting
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