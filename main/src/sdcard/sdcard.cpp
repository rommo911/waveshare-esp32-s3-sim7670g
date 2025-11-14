/**
 * @file      sdcard.cpp
 * @author    Lewis He (lewishe@outlook.com)
 * @license   MIT
 * @copyright Copyright (c) 2022  Shenzhen Xin Yuan Electronic Technology Co., Ltd
 * @date      2022-09-23
 *
 */
#include "FS.h"
#include "SD_MMC.h"
#include "pins.hpp"
#include "wifi/wifi.hpp"
#include "power/power.hpp"
#include "FS.h"
#include <Update.h>
#include "esp_https_ota.h"
#include "esp_ota_ops.h"

#define UPDATE_FILENAME "/esp32-s3-4g.bin"
namespace sdcard
{
    bool sdcardOK = false;
    bool shutdownSdcard()
    {
        SD_MMC.end();
        sdcardOK = false;
        return true;
    }

    bool setupSdcard()
    {
        if (sdcardOK)
        {
            return true;
        }
        pinMode(SD_CD_PIN, INPUT);
        // if (digitalRead(SD_CD_PIN) == LOW)
        // {
        //     Serial.println("No SD_MMC card attached");
        //     return false;
        // }
        SD_MMC.setPins(SDMMC_CLK, SDMMC_CMD, SDMMC_DATA);
        delay(200);
        if (!SD_MMC.begin("/sdcard", true, false, BOARD_MAX_SDMMC_FREQ))
        {
            mqttLogger.println("ERROR: SD Card Mount failed!");
            SD_MMC.end();
            return false;
        }
        uint8_t cardType = SD_MMC.cardType();

        if (cardType == CARD_NONE)
        {
            Serial.println("No SD_MMC card attached");
            SD_MMC.end();
            return false;
        }

        Serial.print("SD_MMC Card Type: ");
        if (cardType == CARD_MMC)
        {
            Serial.println("MMC");
        }
        else if (cardType == CARD_SD)
        {
            Serial.println("SDSC");
        }
        else if (cardType == CARD_SDHC)
        {
            Serial.println("SDHC");
        }
        else
        {
            Serial.println("UNKNOWN");
        }

        uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
        mqttLogger.printf("SD_MMC Card Size: %lluMB\n", cardSize);
        sdcardOK = true;
        return true;
    }

    // perform the actual update from a given stream
    bool performUpdate(Stream &updateSource, size_t updateSize)
    {
        if (Update.begin(updateSize, U_FLASH))
        {
            size_t written = Update.writeStream(updateSource);
            if (written == updateSize)
            {
                Serial.println("Written : " + String(written) + " successfully");
            }
            else
            {
                Serial.println("Written only : " + String(written) + "/" + String(updateSize) + ". Retry?");
            }
            if (Update.end())
            {
                Serial.println("OTA via SD done!");
                if (Update.isFinished())
                {
                    Serial.println("Update successfully completed!");
                    return true;
                }
                else
                {
                    Serial.println("Update not finished? Something went wrong!");
                    return false;
                }
            }
            else
            {
                Serial.println("Error Occurred. Error #: " + String(Update.getError()));
                return false;
            }
        }
        else
        {
            Serial.println("Not enough space to begin OTA");
            return false;
        }
        return false;
    }

    // check given FS for valid UPDATE_FILENAME and perform update if available
    bool updateFromFS(fs::FS &fs)
    {
        File updateBin = fs.open(UPDATE_FILENAME);
        if (updateBin)
        {
            if (updateBin.isDirectory())
            {
                Serial.println("Error, " UPDATE_FILENAME " is not a file");
                updateBin.close();
                return false;
            }
            Serial.println("Update found");
            // print file details
            Serial.printf("file name %s\n", updateBin.name());
            Serial.printf("file size %d\n", updateBin.size());
            // Serial.printf("md5 %s\n", updateBin.md5)
            size_t updateSize = updateBin.size();

            // atleast 1 megabyte
            if (updateSize > (1024U * 1024U) && updateSize < (4U * 1024U * 1024U))
            {

                const esp_partition_t *running = esp_ota_get_running_partition();
                esp_app_desc_t new_app_info;
                Serial.println("Try to start update");
                // check current version with downloading
                const size_t header_size = sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t);
                char buff[header_size + 1];
                updateBin.readBytes(buff, header_size);
                memcpy(&new_app_info, &buff[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)], sizeof(esp_app_desc_t));
                ESP_LOGI("SD OTA", "New firmware version: %s", new_app_info.version);
                esp_app_desc_t running_app_info;
                if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK)
                {
                    ESP_LOGI("SD OTA", "Running firmware version: %s", running_app_info.version);
                }
                const esp_partition_t *last_invalid_app = esp_ota_get_last_invalid_partition();
                esp_app_desc_t invalid_app_info;
                if (esp_ota_get_partition_description(last_invalid_app, &invalid_app_info) == ESP_OK)
                {
                    ESP_LOGI("SD OTA", "Last invalid firmware version: %s", invalid_app_info.version);
                }

                // check current version with last invalid partition
                if (last_invalid_app != NULL)
                {
                    if (memcmp(invalid_app_info.version, new_app_info.version, sizeof(new_app_info.version)) == 0)
                    {
                        ESP_LOGW("SD OTA", "New version is the same as invalid version.");
                        ESP_LOGW("SD OTA", "Previously, there was an attempt to launch the firmware with %s version, but it failed.", invalid_app_info.version);
                        ESP_LOGW("SD OTA", "The firmware has been rolled back to the previous version.");
                        updateBin.close();
                        fs.rename(UPDATE_FILENAME, UPDATE_FILENAME ".done");
                        return false;
                    }
                }

                if (memcmp(new_app_info.version, running_app_info.version, sizeof(new_app_info.version)) == 0)
                {
                    ESP_LOGW("SD OTA", "Current running version is the same as a new. We will not continue the update.");
                    updateBin.close();
                    fs.rename(UPDATE_FILENAME, UPDATE_FILENAME ".done");
                    return false;
                }

                bool ok = performUpdate(updateBin, updateSize);
                updateBin.close();
                if (ok)
                {
                    // when finished remove the binary from sd card to indicate end of the process
                    fs.rename(UPDATE_FILENAME, UPDATE_FILENAME ".done");
                    return true;
                }
                else
                {
                    Serial.println("Update failed");
                    return false;
                }
            }
            else
            {
                Serial.println("Error, file is empty");
                return false;
            }
        }
        else
        {
            Serial.println("no " UPDATE_FILENAME " from sd root");
            return false;
        }
    }

    bool checkForupdatefromSD()
    {
        setupSdcard();
        if (!sdcardOK)
        {
            return false;
        }
        return updateFromFS(SD_MMC);
    }
}