#pragma once
#include <Arduino.h>


const gpio_num_t CAM_PIN = GPIO_NUM_46;
const gpio_num_t I2C_SDA_PIN = GPIO_NUM_9; //
const gpio_num_t I2C_SCL_PIN = GPIO_NUM_10; //
const gpio_num_t MOTION_INTRRUPT_PIN = GPIO_NUM_11; //

const gpio_num_t VBUS_PMU_INPUT_PIN = GPIO_NUM_14;
const gpio_num_t PIXEL_LED_PIN = GPIO_NUM_38;


#define I2C_SDA_POWER                     (3)
#define I2C_SCL_POWER                     (2)

#define BOARD_MODEM_PWR_PIN         (33)
#define BOARD_MODEM_DTR_PIN         (45)
#define BOARD_MODEM_RI_PIN          (40)
#define BOARD_MODEM_RXD_PIN         (18)
#define BOARD_MODEM_TXD_PIN         (17)

const int SDMMC_CLK = 5;
const int SDMMC_CMD = 4;
const int SDMMC_DATA = 6;
const int SD_CD_PIN = 46;