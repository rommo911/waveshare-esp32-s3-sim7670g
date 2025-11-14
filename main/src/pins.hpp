#pragma once
#include <Arduino.h>


const gpio_num_t I2C_SDA_PIN = GPIO_NUM_39; //
const gpio_num_t I2C_SCL_PIN = GPIO_NUM_41; //
const gpio_num_t MOTION_INTRRUPT_PIN = GPIO_NUM_7; //
const gpio_num_t CAM_PIN = GPIO_NUM_8;
const gpio_num_t VBUS_INPUT_PIN = GPIO_NUM_9;

const gpio_num_t BOOT_INPUT_PIN = GPIO_NUM_0;
const gpio_num_t PIXEL_LED_PIN = GPIO_NUM_38;

const gpio_num_t I2C_SDA_POWER = GPIO_NUM_3;
const gpio_num_t I2C_SCL_POWER = GPIO_NUM_2;

const gpio_num_t SDMMC_CLK = GPIO_NUM_5;
const gpio_num_t SDMMC_CMD = GPIO_NUM_4;
const gpio_num_t SDMMC_DATA = GPIO_NUM_6;
const gpio_num_t SD_CD_PIN = GPIO_NUM_46;



#define BOARD_MODEM_PWR_PIN         (33)
#define BOARD_MODEM_DTR_PIN         (45)
#define BOARD_MODEM_RI_PIN          (40)
#define BOARD_MODEM_RXD_PIN         (18)
#define BOARD_MODEM_TXD_PIN         (17)
