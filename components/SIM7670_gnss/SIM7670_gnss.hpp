/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
//
// Created on: 23.08.2022
// Author: franz


#pragma once

#include <memory>
#include "cxx_include/esp_modem_dce_factory.hpp"
#include "cxx_include/esp_modem_dce_module.hpp"
#include "sim76xx_gps.h"
#include <time.h>
#include <list>

/**
 * @brief A custom SIM7670 class with added GNSS capabilities.
 *
 * This class extends the `esp_modem::SIM7600` class from the `esp-modem`
 * component to include methods for retrieving GNSS (Global Navigation
 * Satellite System) data.
 */

/**
 * @brief Structure to hold an SMS message data.
 */
struct sms_t {
    int index;              /*!< Message index in storage */
    std::string status;     /*!< "REC READ", "REC UNREAD", etc. */
    std::string sender;     /*!< Sender's phone number */
    std::string timestamp;  /*!< Service center timestamp */
    std::string content;    /*!< The message body */
};

/**
 * @brief Modem functionality levels, corresponding to the AT+CFUN command.
 */
enum class functionality_level_t {
    MINIMUM = 0,    /*!< Minimum functionality (AT+CFUN=0). RF is disabled, but SIM is accessible. Ideal for low-power states where network is not needed. */
    FULL = 1,       /*!< Full functionality (AT+CFUN=1). Default mode with RF enabled. */
    FLIGHT_MODE = 4 /*!< Flight mode (AT+CFUN=4). RF is disabled, but SIM remains active. */
};

/**
 * @brief Ring Indicator (RI) pin behavior modes, corresponding to the AT+CFGRI command.
 */
enum class ring_indicator_mode_t {
    SMS_CALL_ONLY = 0, /*!< RI pin will only be asserted for incoming calls and new SMS messages. */
    SMS_CALL_URC = 1   /*!< RI pin will be asserted for calls, SMS, and other Unsolicited Result Codes (URCs). This is the most versatile option. */
};

class SIM7670_gnss: public esp_modem::SIM7600 {
private:
    int dtr_pin = -1;

public:
    using SIM7600::SIM7600;

    /**
     * @brief Gets the GNSS information from the modem.
     *
     * This method sends the "AT+CGNSINF" command to the modem and parses the
     * response, filling the provided struct with location, time, and satellite data.
     *
     * @param[out] gps A reference to a `sim76xx_gps_t` struct to store the GNSS data.
     * @return esp_modem::command_result::OK on success,
     *         esp_modem::command_result::FAIL on failure.
     */
    esp_modem::command_result get_gnss_information_sim76xx(sim76xx_gps_t &gps);

    /**
     * @brief Placeholder for answering a call.
     *
     * @note The current implementation of this function in the .cpp file appears
     *       to be incorrectly sending a GNSS command ("AT+CGNSINF") instead of
     *       a command to answer a call (e.g., "ATA"). This should be reviewed.
     *
     * @return esp_modem::command_result indicating the outcome.
     */
    esp_modem::command_result answer_call();

    /**
     * @brief Sets the SMS message format.
     * @param text_mode True for text mode, false for PDU mode.
     * @return esp_modem::command_result::OK on success.
     */
    esp_modem::command_result set_sms_text_mode(bool text_mode);

    /**
     * @brief Gets a list of unread SMS messages.
     *
     * This method sends "AT+CMGL=\"REC UNREAD\"" and parses the response.
     *
     * @param[out] sms_list A list to be populated with unread SMS messages.
     * @return esp_modem::command_result::OK on success.
     */
    esp_modem::command_result get_unread_sms_list(std::list<sms_t> &sms_list);

    /**
     * @brief Deletes an SMS message at a specific index.
     *
     * @param index The index of the message to delete.
     * @return esp_modem::command_result::OK on success.
     */
    esp_modem::command_result delete_sms(int index);

    /**
     * @brief Sets the modem to automatically answer incoming calls.
     *
     * Uses the ATS0 command.
     * @param rings The number of rings before auto-answering. Set to 0 to disable.
     * @return esp_modem::command_result::OK on success.
     */
    esp_modem::command_result set_auto_answer(int rings);

    /**
     * @brief Sets the modem's functionality level (power mode).
     *
     * Uses the AT+CFUN command to switch between minimum, full, and flight modes.
     * @param level The functionality level to set.
     * @return esp_modem::command_result::OK on success.
     */
    esp_modem::command_result set_functionality_level(functionality_level_t level);

    /**
     * @brief Enables or disables the modem's sleep mode.
     *
     * Uses the AT+CSCLK command. When enabled, the modem can enter a low-power
     * state when idle. The DTR pin is often used to wake the modem.
     * @param enable True to enable sleep mode (AT+CSCLK=1), false to disable (AT+CSCLK=0).
     * @return esp_modem::command_result::OK on success.
     */
    esp_modem::command_result enable_sleep_mode(bool enable);

    /**
     * @brief Powers down the modem.
     *
     * Uses the AT+CPOWD=1 command for a normal power-down.
     * @return esp_modem::command_result::OK on success.
     */
    esp_modem::command_result power_down();

    /**
     * @brief Configures the DTR pin and initializes it.
     *
     * @param gpio_num The GPIO number to be used as the DTR pin.
     * @return esp_modem::command_result::OK on success.
     */
    esp_modem::command_result set_dtr_pin(int gpio_num);

    /**
     * @brief Wakes the modem by toggling the DTR pin.
     *
     * This function checks if sleep mode was previously enabled. If so, it
     * pulls the DTR pin low to wake the modem, waits briefly, and then sets
     * it high again to allow the modem to go back to sleep when idle.
     * @return esp_modem::command_result::OK on success, FAIL if DTR pin is not set.
     */
    esp_modem::command_result wake_via_dtr(bool wake);

    /**
     * @brief Gets the network time from the modem.
     *
     * Uses the AT+CCLK? command to get the current time and timezone offset.
     * @param[out] time A struct tm to be filled with the network time (in UTC).
     * @return esp_modem::command_result::OK on success.
     */
    esp_modem::command_result get_network_time(struct tm &time);

    /**
     * @brief Syncs the ESP32 system time with the modem's network time.
     *
     * This function fetches the UTC time from the modem, sets the system clock,
     * and configures the specified time zone.
     * @param timezone_posix POSIX string for the desired timezone (e.g., "CET-1CEST,M3.5.0,M10.5.0/3").
     * @return True if time was synced successfully, false otherwise.
     */
    bool sync_system_time(const std::string& timezone_posix);

    /**
     * @brief Configures the behavior of the Ring Indicator (RI) pin.
     *
     * This function allows setting the RI pin to assert on specific events,
     * enabling the host MCU to wake from sleep.
     * @param mode The desired behavior for the RI pin.
     * @return esp_modem::command_result::OK on success.
     */
    esp_modem::command_result set_ring_indicator_mode(ring_indicator_mode_t mode);
};

/**
 * @brief Data Communication Equipment (DCE) for the `SIM7670_gnss` class.
 *
 * This class acts as a wrapper around the `SIM7670_gnss` device, forwarding
 * both the standard modem commands and the custom GNSS commands. It simplifies
 * interaction with the modem device.
 */
class DCE_gnss : public esp_modem::DCE_T<SIM7670_gnss> {
public:

    using DCE_T<SIM7670_gnss>::DCE_T;
#define ESP_MODEM_DECLARE_DCE_COMMAND(name, return_type, num, ...) \
    template <typename ...Agrs> \
    esp_modem::return_type name(Agrs&&... args)   \
    {   \
        return device->name(std::forward<Agrs>(args)...); \
    }

    DECLARE_ALL_COMMAND_APIS(forwards name(...)
    {
        device->name(...);
    } )

#undef ESP_MODEM_DECLARE_DCE_COMMAND

    /**
     * @brief Gets the GNSS information from the modem.
     *
     * This method forwards the call to the underlying `SIM7670_gnss` device.
     *
     * @param[out] gps A reference to a `sim76xx_gps_t` struct to store the GNSS data.
     * @return esp_modem::command_result::OK on success,
     *         esp_modem::command_result::FAIL on failure.
     */
    esp_modem::command_result get_gnss_information_sim76xx(sim76xx_gps_t &gps);

    /**
     * @brief Forwards the `answer_call` command to the device.
     *
     * @return esp_modem::command_result indicating the outcome.
     */
    esp_modem::command_result answer_call();

    /**
     * @brief Forwards the `set_sms_text_mode` command to the device.
     * @param text_mode True for text mode, false for PDU mode.
     * @return esp_modem::command_result::OK on success.
     */
    esp_modem::command_result set_sms_text_mode(bool text_mode);

    /**
     * @brief Forwards the `get_unread_sms_list` command to the device.
     *
     * @param[out] sms_list A list to be populated with unread SMS messages.
     * @return esp_modem::command_result::OK on success.
     */
    esp_modem::command_result get_unread_sms_list(std::list<sms_t> &sms_list);

    /**
     * @brief Forwards the `delete_sms` command to the device.
     *
     * @param index The index of the message to delete.
     * @return esp_modem::command_result::OK on success.
     */
    esp_modem::command_result delete_sms(int index);

    /**
     * @brief Forwards the `set_auto_answer` command to the device.
     *
     * @param rings The number of rings before auto-answering. Set to 0 to disable.
     * @return esp_modem::command_result::OK on success.
     */
    esp_modem::command_result set_auto_answer(int rings);

    /**
     * @brief Forwards the `set_functionality_level` command to the device.
     *
     * @param level The functionality level to set.
     * @return esp_modem::command_result::OK on success.
     */
    esp_modem::command_result set_functionality_level(functionality_level_t level);

    /**
     * @brief Forwards the `enable_sleep_mode` command to the device.
     *
     * @param enable True to enable sleep mode, false to disable.
     * @return esp_modem::command_result::OK on success.
     */
    esp_modem::command_result enable_sleep_mode(bool enable);

    /**
     * @brief Forwards the `power_down` command to the device.
     *
     * @return esp_modem::command_result::OK on success.
     */
    esp_modem::command_result power_down();

    /**
     * @brief Forwards the `set_dtr_pin` command to the device.
     *
     * @param gpio_num The GPIO number for the DTR pin.
     * @return esp_modem::command_result::OK on success.
     */
    esp_modem::command_result set_dtr_pin(int gpio_num);

    /**
     * @brief Forwards the `wake_via_dtr` command to the device.
     *
     * @return esp_modem::command_result::OK on success.
     */
    esp_modem::command_result wake_via_dtr(bool wake);

    /**
     * @brief Forwards the `get_network_time` command to the device.
     *
     * @param[out] time A struct tm to be filled with the network time (in UTC).
     * @return esp_modem::command_result::OK on success.
     */
    esp_modem::command_result get_network_time(struct tm &time);

    /**
     * @brief Forwards the `sync_system_time` command to the device.
     *
     * @param timezone_posix POSIX string for the desired timezone.
     * @return True if time was synced successfully, false otherwise.
     */
    bool sync_system_time(const std::string& timezone_posix);

    /**
     * @brief Forwards the `set_ring_indicator_mode` command to the device.
     *
     * @param mode The desired behavior for the RI pin.
     * @return esp_modem::command_result::OK on success.
     */
    esp_modem::command_result set_ring_indicator_mode(ring_indicator_mode_t mode);
};


/**
 * @brief Creates a `DCE_gnss` object.
 *
 * This factory function simplifies the creation of a `DCE_gnss` instance.
 * It uses a local factory to build the specialized DCE object.
 *
 * @param config Pointer to the DCE configuration struct.
 * @param dte Shared pointer to the DTE (Data Terminal Equipment).
 * @param netif Pointer to the network interface.
 * @return A `std::unique_ptr` to the created `DCE_gnss` object.
 */
std::unique_ptr<DCE_gnss> create_SIM7670_GNSS_dce(const esp_modem::dce_config *config,
        std::shared_ptr<esp_modem::DTE> dte,
        esp_netif_t *netif);
