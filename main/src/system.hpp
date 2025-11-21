/**
 * @file system.hpp
 * @author rami zayat
 * @brief
 * @version 0.1
 * @date 2025-11-21
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once
#include <stdint.h>
void light_sleep(uint16_t seconds);
void set_ota_valid(bool valid);
bool check_rollback();