/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * @file SIM7670_gnss.cpp
 * @brief Implements the GNSS functionality for the SIM7670 modem.
 * @author franz (original)
 */
#include <string_view>
#include <charconv>
#include <sstream>
#include <list>
#include "sdkconfig.h"
#include "esp_log.h"
#include "cxx_include/esp_modem_dte.hpp"
#include "cxx_include/esp_modem_dce.hpp"
#include "esp_modem_config.h"
#include "cxx_include/esp_modem_api.hpp"
#include "cxx_include/esp_modem_command_library_utils.hpp"
#include "SIM7670_gnss.hpp"
#include "driver/gpio.h"
#include <sys/time.h>

constexpr auto const TAG = "SIM7670_gnss";

/**
 * @brief RTC variable to persist the modem's sleep state across deep sleep cycles.
 */
static RTC_DATA_ATTR bool modem_is_in_sleep_mode = false;

namespace gnss_factory
{
    using namespace esp_modem;
    using namespace dce_factory;
    /**
     * @brief A local factory class for creating DCE_gnss objects.
     *
     * This class specializes the generic `Factory` to build instances of
     * `DCE_gnss`, which is the Data Communication Equipment object for the
     * SIM7670 modem with GNSS capabilities.
     */
    class LocalFactory : public Factory
    {
        using DCE_gnss_ret = std::unique_ptr<DCE_gnss>; // this custom Factory manufactures only unique_ptr<DCE>'s
    public:
        /**
         * @brief Creates a unique_ptr to a DCE_gnss object.
         * @param config DCE configuration.
         * @param dte Shared pointer to the DTE.
         * @param netif Network interface pointer.
         * @return A unique_ptr to the created DCE_gnss object.
         */
        static DCE_gnss_ret create(const dce_config *config, std::shared_ptr<DTE> dte, esp_netif_t *netif)
        {
            return Factory::build_generic_DCE<SIM7670_gnss, DCE_gnss, DCE_gnss_ret>(config, std::move(dte), netif);
        }
    };

} // namespace gnss_factory

std::unique_ptr<DCE_gnss> create_SIM7670_GNSS_dce(const esp_modem::dce_config *config,
                                                  std::shared_ptr<esp_modem::DTE> dte,
                                                  esp_netif_t *netif)
{
    return gnss_factory::LocalFactory::create(config, std::move(dte), netif);
}
/**
 * @brief Parses the GNSS information from a `AT+CGNSINF` response.
 *
 * This is a library-style function that sends the `AT+CGNSINF` command to the modem
 * and parses the resulting string to populate a `sim76xx_gps_t` struct.
 *
 * @param[in] t Pointer to the `CommandableIf` interface for sending commands.
 * @param[out] gps Reference to the `sim76xx_gps_t` struct to be filled with data.
 * @return `esp_modem::command_result::OK` on successful parsing.
 * @return `esp_modem::command_result::FAIL` if the command fails or parsing is unsuccessful.
 */
esp_modem::command_result get_gnss_information_sim76xx_lib(esp_modem::CommandableIf *t, sim76xx_gps_t &gps)
{
    ESP_LOGV(TAG, "%s", __func__);
    std::string str_out;
    auto ret = esp_modem::dce_commands::generic_get_string(t, "AT+CGNSINF\r", str_out);
    if (ret != esp_modem::command_result::OK)
    {
        return ret;
    }
    std::string_view out(str_out);

    constexpr std::string_view pattern = "+CGNSINF: ";
    if (out.find(pattern) == std::string_view::npos)
    {
        return esp_modem::command_result::FAIL;
    }
    /**
     * Parsing +CGNSINF:
    | **Index** | **Parameter**          | **Unit**           | **Range**                                                                            | **Length** |
    |-----------|------------------------|--------------------|--------------------------------------------------------------------------------------|------------|
    | 1         | GNSS run status        | --                 | 0-1                                                                                  | 1          |
    | 2         | Fix status             | --                 | 0-1                                                                                  | 1          |
    | 3         | UTC date & Time        | yyyyMMddhhmmss.sss | yyyy: [1980,2039] MM : [1,12] dd: [1,31] hh: [0,23] mm: [0,59] ss.sss:[0.000,60.999] | 18         |
    | 4         | Latitude               | ±dd.dddddd         | [-90.000000,90.000000]                                                               | 10         |
    | 5         | Longitude              | ±dd.dddddd         | -180.000000,180.000000]                                                              | 11         |
    | 6         | MSL Altitude           | meters             | [0,999.99]                                                                           | 8          |
    | 7         | Speed Over Ground      | Km/hour            | [0,360.00]                                                                           | 6          |
    | 8         | Course Over Ground     | degrees            | 0,1,2[1]                                                                             | 6          |
    | 9         | Fix Mode               | --                 |                                                                                      | 1          |
    | 10        | Reserved1              |                    |                                                                                      | 0          |
    | 11        | HDOP                   | --                 | [0,99.9]                                                                             | 4          |
    | 12        | PDOP                   | --                 | [0,99.9]                                                                             | 4          |
    | 13        | VDOP                   | --                 | [0,99.9]                                                                             | 4          |
    | 14        | Reserved2              |                    |                                                                                      | 0          |
    | 15        | GPS Satellites in View | --                 | -- [0,99]                                                                            | 2          |
    | 16        | Reserved3              |                    |                                                                                      | 0          |
    | 17        | HPA[2]                 | meters             | [0,9999.9]                                                                           | 6          |
    | 18        | VPA[2]                 | meters             | [0,9999.9]                                                                           | 6          |
     */
    out = out.substr(pattern.size());
    int pos = 0;
    if ((pos = out.find(',')) == std::string::npos)
    {
        return esp_modem::command_result::FAIL;
    }
    // GNSS run status
    int GNSS_run_status;
    if (std::from_chars(out.data(), out.data() + pos, GNSS_run_status).ec == std::errc::invalid_argument)
    {
        return esp_modem::command_result::FAIL;
    }
    gps.run = (gps_run_t)GNSS_run_status;
    out = out.substr(pos + 1);
    if ((pos = out.find(',')) == std::string::npos)
    {
        return esp_modem::command_result::FAIL;
    }
    // Fix status
    {
        std::string_view fix_status = out.substr(0, pos);
        if (fix_status.length() > 1)
        {
            int Fix_status;
            if (std::from_chars(out.data(), out.data() + pos, Fix_status).ec == std::errc::invalid_argument)
            {
                return esp_modem::command_result::FAIL;
            }
            gps.fix = (gps_fix_t)Fix_status;
        }
        else
        {
            gps.fix = GPS_FIX_INVALID;
        }
    } // clean up Fix status
    out = out.substr(pos + 1);
    if ((pos = out.find(',')) == std::string::npos)
    {
        return esp_modem::command_result::FAIL;
    }
    // UTC date &  Time
    {
        std::string_view UTC_date_and_Time = out.substr(0, pos);
        if (UTC_date_and_Time.length() > 1)
        {
            if (std::from_chars(out.data() + 0, out.data() + 4, gps.date.year).ec == std::errc::invalid_argument)
            {
                return esp_modem::command_result::FAIL;
            }
            if (std::from_chars(out.data() + 4, out.data() + 6, gps.date.month).ec == std::errc::invalid_argument)
            {
                return esp_modem::command_result::FAIL;
            }
            if (std::from_chars(out.data() + 6, out.data() + 8, gps.date.day).ec == std::errc::invalid_argument)
            {
                return esp_modem::command_result::FAIL;
            }
            if (std::from_chars(out.data() + 8, out.data() + 10, gps.tim.hour).ec == std::errc::invalid_argument)
            {
                return esp_modem::command_result::FAIL;
            }
            if (std::from_chars(out.data() + 10, out.data() + 12, gps.tim.minute).ec == std::errc::invalid_argument)
            {
                return esp_modem::command_result::FAIL;
            }
            if (std::from_chars(out.data() + 12, out.data() + 14, gps.tim.second).ec == std::errc::invalid_argument)
            {
                return esp_modem::command_result::FAIL;
            }
            if (std::from_chars(out.data() + 15, out.data() + 18, gps.tim.thousand).ec == std::errc::invalid_argument)
            {
                return esp_modem::command_result::FAIL;
            }
        }
        else
        {
            gps.date.year = 0;
            gps.date.month = 0;
            gps.date.day = 0;
            gps.tim.hour = 0;
            gps.tim.minute = 0;
            gps.tim.second = 0;
            gps.tim.thousand = 0;
        }

    } // clean up UTC date &  Time
    out = out.substr(pos + 1);
    if ((pos = out.find(',')) == std::string::npos)
    {
        return esp_modem::command_result::FAIL;
    }
    // Latitude
    {
        std::string_view Latitude = out.substr(0, pos);
        if (Latitude.length() > 1)
        {
            gps.latitude = std::stof(std::string(out.substr(0, pos)));
        }
        else
        {
            gps.latitude = 0;
        }
    } // clean up Latitude
    out = out.substr(pos + 1);
    if ((pos = out.find(',')) == std::string::npos)
    {
        return esp_modem::command_result::FAIL;
    }
    // Longitude
    {
        std::string_view Longitude = out.substr(0, pos);
        if (Longitude.length() > 1)
        {
            gps.longitude = std::stof(std::string(out.substr(0, pos)));
        }
        else
        {
            gps.longitude = 0;
        }
    } // clean up Longitude
    out = out.substr(pos + 1);
    if ((pos = out.find(',')) == std::string::npos)
    {
        return esp_modem::command_result::FAIL;
    }
    // Altitude
    {
        std::string_view Altitude = out.substr(0, pos);
        if (Altitude.length() > 1)
        {
            gps.altitude = std::stof(std::string(out.substr(0, pos)));
        }
        else
        {
            gps.altitude = 0;
        }
    } // clean up Altitude
    out = out.substr(pos + 1);
    if ((pos = out.find(',')) == std::string::npos)
    {
        return esp_modem::command_result::FAIL;
    }
    // Speed Over Ground Km/hour
    {
        std::string_view gps_speed = out.substr(0, pos);
        if (gps_speed.length() > 1)
        {
            gps.speed = std::stof(std::string(gps_speed));
        }
        else
        {
            gps.speed = 0;
        }
    } // clean up gps_speed
    out = out.substr(pos + 1);
    if ((pos = out.find(',')) == std::string::npos)
    {
        return esp_modem::command_result::FAIL;
    }
    // Course Over Ground degrees
    {
        std::string_view gps_cog = out.substr(0, pos);
        if (gps_cog.length() > 1)
        {
            gps.cog = std::stof(std::string(gps_cog));
        }
        else
        {
            gps.cog = 0;
        }
    } // clean up gps_cog
    out = out.substr(pos + 1);
    if ((pos = out.find(',')) == std::string::npos)
    {
        return esp_modem::command_result::FAIL;
    }
    // Fix Mode
    {
        std::string_view FixModesubstr = out.substr(0, pos);
        if (FixModesubstr.length() > 1)
        {
            int Fix_Mode;
            if (std::from_chars(out.data(), out.data() + pos, Fix_Mode).ec == std::errc::invalid_argument)
            {
                return esp_modem::command_result::FAIL;
            }
            gps.fix_mode = (gps_fix_mode_t)Fix_Mode;
        }
        else
        {
            gps.fix_mode = GPS_MODE_INVALID;
        }
    } // clean up Fix Mode
    out = out.substr(pos + 1);
    if ((pos = out.find(',')) == std::string::npos)
    {
        return esp_modem::command_result::FAIL;
    }
    out = out.substr(pos + 1);
    if ((pos = out.find(',')) == std::string::npos)
    {
        return esp_modem::command_result::FAIL;
    }
    // HDOP
    {
        std::string_view HDOP = out.substr(0, pos);
        if (HDOP.length() > 1)
        {
            gps.dop_h = std::stof(std::string(HDOP));
        }
        else
        {
            gps.dop_h = 0;
        }
    } // clean up HDOP
    out = out.substr(pos + 1);
    if ((pos = out.find(',')) == std::string::npos)
    {
        return esp_modem::command_result::FAIL;
    }
    // PDOP
    {
        std::string_view PDOP = out.substr(0, pos);
        if (PDOP.length() > 1)
        {
            gps.dop_p = std::stof(std::string(PDOP));
        }
        else
        {
            gps.dop_p = 0;
        }
    } // clean up PDOP
    out = out.substr(pos + 1);
    if ((pos = out.find(',')) == std::string::npos)
    {
        return esp_modem::command_result::FAIL;
    }
    // VDOP
    {
        std::string_view VDOP = out.substr(0, pos);
        if (VDOP.length() > 1)
        {
            gps.dop_v = std::stof(std::string(VDOP));
        }
        else
        {
            gps.dop_v = 0;
        }
    } // clean up VDOP
    out = out.substr(pos + 1);
    if ((pos = out.find(',')) == std::string::npos)
    {
        return esp_modem::command_result::FAIL;
    }
    out = out.substr(pos + 1);
    if ((pos = out.find(',')) == std::string::npos)
    {
        return esp_modem::command_result::FAIL;
    }
    // sats_in_view
    {
        std::string_view sats_in_view = out.substr(0, pos);
        if (sats_in_view.length() > 1)
        {
            if (std::from_chars(out.data(), out.data() + pos, gps.sat.num).ec == std::errc::invalid_argument)
            {
                return esp_modem::command_result::FAIL;
            }
        }
        else
        {
            gps.sat.num = 0;
        }
    } // clean up sats_in_view

    out = out.substr(pos + 1);
    if ((pos = out.find(',')) == std::string::npos)
    {
        return esp_modem::command_result::FAIL;
    }
    out = out.substr(pos + 1);
    if ((pos = out.find(',')) == std::string::npos)
    {
        return esp_modem::command_result::FAIL;
    }
    // HPA
    {
        std::string_view HPA = out.substr(0, pos);
        if (HPA.length() > 1)
        {
            gps.hpa = std::stof(std::string(HPA));
        }
        else
        {
            gps.hpa = 0;
        }
    } // clean up HPA
    out = out.substr(pos + 1);
    // VPA
    {
        std::string_view VPA = out.substr(0, pos);
        if (VPA.length() > 1)
        {
            gps.vpa = std::stof(std::string(VPA));
        }
        else
        {
            gps.vpa = 0;
        }
    } // clean up VPA

    return esp_modem::command_result::OK;
}

/*! @copydoc SIM7670_gnss::get_gnss_information_sim76xx */
esp_modem::command_result SIM7670_gnss::get_gnss_information_sim76xx(sim76xx_gps_t &gps)
{
    return get_gnss_information_sim76xx_lib(dte.get(), gps);
}

/*! @copydoc DCE_gnss::get_gnss_information_sim76xx */
esp_modem::command_result DCE_gnss::get_gnss_information_sim76xx(sim76xx_gps_t &gps)
{
    return device->get_gnss_information_sim76xx(gps);
}

/**
 * @brief Library-style function to answer a call.
 *
 * @note This function currently sends "AT+CGNSINF\r", which is incorrect for
 *       answering a call. It should be sending "ATA\r". This is a bug.
 *
 * @param[in] t Pointer to the `CommandableIf` interface for sending commands.
 * @return `esp_modem::command_result::OK` on success.
 * @return `esp_modem::command_result::FAIL` on failure.
 */
esp_modem::command_result answer_call_lib(esp_modem::CommandableIf *t)
{
    std::string str_out;
    // FIXME: This should be "ATA\r" to answer a call, not a GNSS command.
    auto ret = esp_modem::dce_commands::generic_command(t, "ATA\r", "OK", "ERROR", 1500);
    if (ret != esp_modem::command_result::OK)
    {
        return ret;
    }
    return ret;
}

/*! @copydoc SIM7670_gnss::answer_call */
esp_modem::command_result SIM7670_gnss::answer_call()
{
    return answer_call_lib(dte.get());
}

/*! @copydoc DCE_gnss::answer_call */
esp_modem::command_result DCE_gnss::answer_call()
{
    return device->answer_call();
}

esp_modem::command_result set_sms_text_mode_lib(esp_modem::CommandableIf *t, bool text_mode)
{
    int mode = text_mode ? 1 : 0;
    return esp_modem::dce_commands::generic_command(t, "AT+CMGF=" + std::to_string(mode) + "\r", "OK", "ERROR", 1000);
}

/*! @copydoc SIM7670_gnss::set_sms_text_mode */
esp_modem::command_result SIM7670_gnss::set_sms_text_mode(bool text_mode)
{
    return set_sms_text_mode_lib(dte.get(), text_mode);
}

/*! @copydoc DCE_gnss::set_sms_text_mode */
esp_modem::command_result DCE_gnss::set_sms_text_mode(bool text_mode)
{
    return device->set_sms_text_mode(text_mode);
}

esp_modem::command_result get_unread_sms_list_lib(esp_modem::CommandableIf *t, std::list<sms_t> &sms_list)
{
    std::string str_out;
    ESP_LOGI(TAG, "asking SMS unread list");
    set_sms_text_mode_lib(t, true);
    auto ret = esp_modem::dce_commands::at_raw(t, "AT+CMGL=\"REC UNREAD\"\r", str_out, "OK", "FAIL", 5000); // esp_modem::dce_commands::generic_get_string(t, "AT+CMGL=\"REC UNREAD\"\r", str_out, 5000);
    if (ret != esp_modem::command_result::OK)
    {
        ESP_LOGE(TAG, "get sms failed, ret = %d , str = %s", (int)ret, str_out.c_str());
        return ret;
    }

    if (str_out.find("OK") == std::string::npos)
    {
        ESP_LOGE(TAG, "get sms failed not find OK, ret = %d , strout=%s", (int)ret, str_out.c_str());
        return esp_modem::command_result::FAIL;
    }
    //ESP_LOGI(TAG, "get sms  strout=%s", str_out.c_str());

    sms_list.clear();
    std::stringstream ss(str_out);
    std::string line;

    while (std::getline(ss, line))
    {
        if (line.rfind("+CMGL: ", 0) == 0)
        {
            sms_t current_sms;
            std::string header = line;
            // Try to get message content from the next line
            if (std::getline(ss, line))
            {
                current_sms.content = line;
                // Trim trailing \r if present
                if (!current_sms.content.empty() && current_sms.content.back() == '\r')
                {
                    current_sms.content.pop_back();
                }
            }

            // Parse header: +CMGL: <index>,<stat>,<oa>,[<alpha>],<scts>
            std::stringstream header_ss(header.substr(7)); // Skip "+CMGL: "
            std::string segment;

            // 1. Index
            if (!std::getline(header_ss, segment, ','))
                continue;
            current_sms.index = std::stoi(segment);

            // 2. Status
            if (!std::getline(header_ss, segment, ','))
                continue;
            current_sms.status = segment;

            // 3. Sender
            if (!std::getline(header_ss, segment, ','))
                continue;
            current_sms.sender = segment;

            // 4. Alpha (skip)
            if (!std::getline(header_ss, segment, ','))
                continue;

            // 5. Timestamp
            if (!std::getline(header_ss, segment))
                continue;
            current_sms.timestamp = segment;
            // Remove quotes from strings
            current_sms.status.erase(std::remove(current_sms.status.begin(), current_sms.status.end(), '"'), current_sms.status.end());
            current_sms.sender.erase(std::remove(current_sms.sender.begin(), current_sms.sender.end(), '"'), current_sms.sender.end());
            current_sms.timestamp.erase(std::remove(current_sms.timestamp.begin(), current_sms.timestamp.end(), '"'), current_sms.timestamp.end());
            sms_list.push_back(current_sms);
        }
    }
    if (sms_list.size() > 0)
    {
        ESP_LOGI(TAG, "got %d unread sms ", (int)sms_list.size());
    }
    else
    {
        ESP_LOGI(TAG, "no unread sms found");
    }
    return esp_modem::command_result::OK;
}

/*! @copydoc SIM7670_gnss::get_unread_sms_list */
esp_modem::command_result SIM7670_gnss::get_unread_sms_list(std::list<sms_t> &sms_list)
{
    return get_unread_sms_list_lib(dte.get(), sms_list);
}

/*! @copydoc DCE_gnss::get_unread_sms_list */
esp_modem::command_result DCE_gnss::get_unread_sms_list(std::list<sms_t> &sms_list)
{
    return device->get_unread_sms_list(sms_list);
}

esp_modem::command_result delete_sms_lib(esp_modem::CommandableIf *t, int index)
{
    auto ret = esp_modem::dce_commands::generic_command(t, "AT+CMGD=" + std::to_string(index) + "\r", "OK", "ERROR", 5000);
    if (ret != esp_modem::command_result::OK)
    {
        ESP_LOGE(TAG, "delete sms failed, ret = %d", (int)ret);
    }
    ESP_LOGI(TAG, "delete sms ret = %d", (int)ret);
    return ret;
}

/*! @copydoc SIM7670_gnss::delete_sms */
esp_modem::command_result SIM7670_gnss::delete_sms(int index)
{
    return delete_sms_lib(dte.get(), index);
}

/*! @copydoc DCE_gnss::delete_sms */
esp_modem::command_result DCE_gnss::delete_sms(int index)
{
    return device->delete_sms(index);
}

esp_modem::command_result set_auto_answer_lib(esp_modem::CommandableIf *t, int rings)
{
    return esp_modem::dce_commands::generic_command(t, "ATS0=" + std::to_string(rings) + "\r", "OK", "ERROR", 1000);
}

/*! @copydoc SIM7670_gnss::set_auto_answer */
esp_modem::command_result SIM7670_gnss::set_auto_answer(int rings)
{
    return set_auto_answer_lib(dte.get(), rings);
}

/*! @copydoc DCE_gnss::set_auto_answer */
esp_modem::command_result DCE_gnss::set_auto_answer(int rings)
{
    return device->set_auto_answer(rings);
}

esp_modem::command_result set_functionality_level_lib(esp_modem::CommandableIf *t, functionality_level_t level)
{
    int level_int = static_cast<int>(level);
    return esp_modem::dce_commands::generic_command(t, "AT+CFUN=" + std::to_string(level_int) + ",0\r", "OK", "ERROR", 5000);
}

/*! @copydoc SIM7670_gnss::set_functionality_level */
esp_modem::command_result SIM7670_gnss::set_functionality_level(functionality_level_t level)
{
    return set_functionality_level_lib(dte.get(), level);
}

/*! @copydoc DCE_gnss::set_functionality_level */
esp_modem::command_result DCE_gnss::set_functionality_level(functionality_level_t level)
{
    return device->set_functionality_level(level);
}

esp_modem::command_result enable_terminal_sleep_mode_lib(esp_modem::CommandableIf *t, bool enable)
{
    modem_is_in_sleep_mode = enable;
    int mode = enable ? 1 : 0;
    return esp_modem::dce_commands::generic_command(t, "AT+CSCLK=" + std::to_string(mode) + "\r", "OK", "ERROR", 1000);
}

/*! @copydoc SIM7670_gnss::enable_terminal_sleep_mode */
esp_modem::command_result SIM7670_gnss::enable_terminal_sleep_mode(bool enable)
{
    return enable_terminal_sleep_mode_lib(dte.get(), enable);
}

/*! @copydoc DCE_gnss::enable_terminal_sleep_mode */
esp_modem::command_result DCE_gnss::enable_terminal_sleep_mode(bool enable)
{
    return device->enable_terminal_sleep_mode(enable);
}

esp_modem::command_result power_down_lib(esp_modem::CommandableIf *t)
{
    return esp_modem::dce_commands::generic_command(t, "AT+CPOWD=1\r", "NORMAL POWER DOWN", "ERROR", 1000);
}

/*! @copydoc SIM7670_gnss::power_down */
esp_modem::command_result SIM7670_gnss::power_down()
{
    return power_down_lib(dte.get());
}

/*! @copydoc DCE_gnss::power_down */
esp_modem::command_result DCE_gnss::power_down()
{
    return device->power_down();
}

/*! @copydoc SIM7670_gnss::set_dtr_pin */
esp_modem::command_result SIM7670_gnss::set_dtr_pin(int gpio_num)
{
    dtr_pin = gpio_num;
    if (dtr_pin != -1)
    {
        gpio_set_direction((gpio_num_t)dtr_pin, GPIO_MODE_OUTPUT);
        // Set DTR high by default to allow sleep
        gpio_set_level((gpio_num_t)dtr_pin, 1);
    }
    return esp_modem::command_result::OK;
}

/*! @copydoc DCE_gnss::set_dtr_pin */
esp_modem::command_result DCE_gnss::set_dtr_pin(int gpio_num)
{
    return device->set_dtr_pin(gpio_num);
}

/*! @copydoc SIM7670_gnss::wake_via_dtr */
esp_modem::command_result SIM7670_gnss::wake_via_dtr(bool wake)
{
    if (dtr_pin == -1)
    {
        ESP_LOGE(TAG, "DTR pin not configured. Call set_dtr_pin() first.");
        return esp_modem::command_result::FAIL;
    }
    if (modem_is_in_sleep_mode)
    {
        if (wake)
        {
            ESP_LOGD(TAG, "Waking modem via DTR pin...");
            gpio_set_level((gpio_num_t)dtr_pin, 0); // Pull DTR low to wake
        }
        else
        {
            ESP_LOGD(TAG, "sleeping modem via DTR pin...");
            gpio_set_level((gpio_num_t)dtr_pin, 1); // Set DTR high to allow sleep again
        }
        vTaskDelay(pdMS_TO_TICKS(100));
        return esp_modem::command_result::OK;
    }
    else
    {
        ESP_LOGD(TAG, "Wake via DTR skipped: modem sleep not enabled.");
    }
    return esp_modem::command_result::OK;
}

/*! @copydoc DCE_gnss::wake_via_dtr */
esp_modem::command_result DCE_gnss::wake_via_dtr(bool wake)
{
    return device->wake_via_dtr(wake);
}

esp_modem::command_result get_network_time_lib(esp_modem::CommandableIf *t, struct tm &time)
{
    std::string str_out;
    auto ret = esp_modem::dce_commands::generic_get_string(t, "AT+CTZU=1\r", str_out, 5000);
    if (ret != esp_modem::command_result::OK)
    {
        return ret;
    }

    ret = esp_modem::dce_commands::generic_get_string(t, "AT+CCLK?\r", str_out, 5000);
    if (ret != esp_modem::command_result::OK)
    {
        return ret;
    }

    // Expected format: +CCLK: "yy/MM/dd,hh:mm:ss±zz"
    std::string_view out(str_out);
    constexpr std::string_view pattern = "+CCLK: \"";
    auto pos = out.find(pattern);
    if (pos == std::string_view::npos)
    {
        return esp_modem::command_result::FAIL;
    }
    out.remove_prefix(pos + pattern.length());

    int year, month, day, hour, minute, second, tz_offset_quarters;
    char tz_sign;

    // Using sscanf for robust parsing of the fixed format
    int fields = sscanf(out.data(), "%d/%d/%d,%d:%d:%d%c%d",
                        &year, &month, &day, &hour, &minute, &second, &tz_sign, &tz_offset_quarters);
    ESP_LOGI(TAG, "time from modem is %s", out.data());
    if (year < 25 || year > 35)
    { // Timezone might not be present
        return esp_modem::command_result::FAIL;
    }
    if (fields < 7)
    { // Timezone might not be present
        return esp_modem::command_result::FAIL;
    }
    // Populate struct tm
    time.tm_year = year + 2000; // tm_year is years since 1900
    time.tm_mon = month;        // tm_mon is 0-11
    time.tm_mday = day;
    time.tm_hour = hour;
    time.tm_min = minute;
    time.tm_sec = second;
    return esp_modem::command_result::OK;
}

/*! @copydoc SIM7670_gnss::get_network_time */
esp_modem::command_result SIM7670_gnss::get_network_time(struct tm &time)
{
    return get_network_time_lib(dte.get(), time);
}

/*! @copydoc DCE_gnss::get_network_time */
esp_modem::command_result DCE_gnss::get_network_time(struct tm &time)
{
    return device->get_network_time(time);
}

esp_modem::command_result set_ring_indicator_mode_lib(esp_modem::CommandableIf *t, ring_indicator_mode_t mode)
{
    int mode_int = static_cast<int>(mode);
    return esp_modem::dce_commands::generic_command(t, "AT+CFGRI=" + std::to_string(mode_int) + "\r", "OK", "ERROR", 1000);
}

/*! @copydoc SIM7670_gnss::set_ring_indicator_mode */
esp_modem::command_result SIM7670_gnss::set_ring_indicator_mode(ring_indicator_mode_t mode)
{
    return set_ring_indicator_mode_lib(dte.get(), mode);
}

/*! @copydoc DCE_gnss::set_ring_indicator_mode */
esp_modem::command_result DCE_gnss::set_ring_indicator_mode(ring_indicator_mode_t mode)
{
    return device->set_ring_indicator_mode(mode);
}