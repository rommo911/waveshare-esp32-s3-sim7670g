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

namespace sdcard
{
    bool shutdownSdcard()
    {
        SD_MMC.end();
        return true;
    }

    bool setupSdcard()
    {
        pinMode(SD_CD_PIN, INPUT);
        delay(10);
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
        return true;
    }
}