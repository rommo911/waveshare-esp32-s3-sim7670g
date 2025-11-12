
/**
 * @file      AllFunction.ino
 * @author    Lewis He (lewishe@outlook.com)
 * @license   MIT
 * @copyright Copyright (c) 2022  Shenzhen Xin Yuan Electronic Technology Co.,
 * Ltd
 * @date      2022-09-16
 *
 */
#include "main.hpp"
#include "Preferences.h"
#include "esp32-hal.h"
#include "imu6500/imu_DMP6.hpp"
#include "led.hpp"
#include "modem/modem.hpp"
#include "pins.hpp"
#include "power/power.hpp"
#include "sdcard/sdcard.h"
#include "wifi/wifi.hpp"
#include <Arduino.h>
#include <thread>

bool simulatedMotionTrigger = false;
bool simulatedLowPowerTrigger = false;
bool simulatedCriticalLowPowerTrigger = false;

static uint64_t LastWifiOnTimestamp = 0;

static uint32_t RTC_DATA_ATTR motionCounter;

static imu6500_dmp::MotionDtect_t motionInfo = {};

power::WakeUpReason wu;

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
    led::start_blink(1, led::_rgb(255, 255, 255), led::_rgb(255, 0, 0), 250, 200, 5000);
    delay(5500);
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.printf("boot \n");
  delay(500);
  led::led_init();
  delay(1500);
  // wu = power::Get_wake_reason();
  // power::setupPower();
  bool ret = false;
  // setCpuFrequencyMhz(80);
  // pinMode(CAM_PIN, OUTPUT);
  // turnOnCamera();
  loadTimingPref();
  StartWifi();
  if (sdcard::checkForupdatefromSD())
  {
    Serial.println("update done restarting");
    delay(100);
    ESP.restart();
  }
  led::set_solid(0, led::_rgb(255, 0, 0));
  delay(500);
  led::set_solid(0, led::_rgb(0, 255, 0));
  delay(500);
  led::set_solid(0, led::_rgb(0, 0, 255));
  delay(500);
}

void loopPowerCheck()
{
  if (power::isPowerVBUSOn())
  {
    const float percent = power::getPMU().cellPercent();
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
    // power::getPMU().shutdown();
  }
  if (power::isBatLowLevel() || simulatedLowPowerTrigger)
  {
    simulatedLowPowerTrigger = false;
    mqttLogger.printf("Battery low level detected in main loop \n");
    led::set_solid(0, led::_rgb(2, 0, 0)); // turn off led before sleep
    delay(30);
    power::DeepSleepWith_IMU_PMU_Wake();
  }
}

bool waitForCarhelper = false;
void loopImuMotion()
{
  if (imu6500_dmp::getMotion())
  {
    led::start_blink(0, led::_rgb(0, 0, 100), 0, 150, 200,
                     200); // turn off led before sleep
    delay(150);
  }
  if (power::isPowerVBUSOn())
  {
    waitForCarhelper = true;
    return;
  }
  // wait untill no motion for a while and Vbus removed for a while
  if (NoMotionSince(getNoMotionTimeout()) &&
      NoVbusSince(getSecureModeTimeout()))
  {
    mqttLogger.println("starting secure mode");
    turnOffCamera();
    led::set_solid(0, led::_rgb(0, 0, 2)); // turn off led before sleep
    power::DeepSleepWith_IMU_PMU_Wake();
  }
  else
  {
    if (waitForCarhelper) // only once
    {
      led::start_blink(
          0, led::batteryColor(power::getPMU().cellPercent()), 0,
          100, 2500,
          120 * 1000); // crete blink patter to inform user its waiting
      waitForCarhelper = false;
      mqttLogger.println("waiting for some time after vbus removed");
    }
  }
  return;
}

void loopWifiStatus()
{
  if (false /* key press*/)
  {
    mqttLogger.println("Power key short pressed detected in main loop");
    if (!GetWifiOn())
    {
      led::start_blink(
          1, led::_rgb(0, 50, 50), 0, 200, 2500,
          120 * 1000); // crete blink patter to inform user its waiting
      StartWifi();
      LastWifiOnTimestamp = millis();
    }
  }
  if (GetWifiOn())
  {
    if (((millis() - LastWifiOnTimestamp > getWifiTimeout()) ||
         (!power::isPowerVBUSOn() && power::isBatLowLevel())))
    {
      mqttLogger.println("WiFi on timeout reached, turning off WiFi");
      StopWifi();
    }
  }
}

void loop()
{
  // loopWifiStatus();
  // loopPowerCheck();
  // loopImuMotion();
  delay(10);
}
#include "nvs_flash.h"

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

  setup();
  while (1)
  {
    loop();
  }
}
