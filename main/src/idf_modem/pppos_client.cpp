/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* PPPoS Client Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <Arduino.h>
#include <string.h>
#include <cstring>
#include <iostream>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "esp_log.h"
#include "esp_event.h"
#include "cxx_include/esp_modem_dte.hpp"
#include "esp_modem_config.h"
#include "cxx_include/esp_modem_api.hpp"
#include "mqtt_client.hpp"
#include "pins.hpp"
#include "sdkconfig.h"
#include "cxx_include/esp_modem_api.hpp"
#include "SIM7670_gnss.hpp"
#include "main.hpp"

static const char *TAG = "pppos";
using namespace esp_modem;

class StatusHandler
{
public:
    static constexpr auto IP_Event = SignalGroup::bit0;
    static constexpr auto MQTT_Connect = SignalGroup::bit1;
    static constexpr auto MQTT_Data = SignalGroup::bit2;

    StatusHandler()
    {
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, on_event, this));
    }

    ~StatusHandler()
    {
        esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID, on_event);
    }

    esp_err_t wait_for(decltype(IP_Event) event, int milliseconds)
    {
        return signal.wait_any(event, milliseconds);
    }

    ip_event_t get_ip_event_type()
    {
        return ip_event_type;
    }

private:
    static void on_event(void *arg, esp_event_base_t base, int32_t event, void *data)
    {
        auto *handler = static_cast<StatusHandler *>(arg);
        if (base == IP_EVENT)
        {
            handler->ip_event(event, data);
        }
    }

    void ip_event(int32_t id, void *data)
    {
        if (id == IP_EVENT_PPP_GOT_IP)
        {
            auto *event = (ip_event_got_ip_t *)data;
            ESP_LOGI(TAG, "IP          : " IPSTR, IP2STR(&event->ip_info.ip));
            ESP_LOGI(TAG, "Netmask     : " IPSTR, IP2STR(&event->ip_info.netmask));
            ESP_LOGI(TAG, "Gateway     : " IPSTR, IP2STR(&event->ip_info.gw));
            signal.set(IP_Event);
        }
        else if (id == IP_EVENT_PPP_LOST_IP)
        {
            signal.set(IP_Event);
        }
        ip_event_type = static_cast<ip_event_t>(id);
    }

    esp_modem::SignalGroup signal{};
    ip_event_t ip_event_type;
};

class ModemSim7670
{
public:
    ModemSim7670(int uart_rx_pin = BOARD_MODEM_RXD_PIN, int uart_tx_pin = BOARD_MODEM_TXD_PIN, int dtr_pin = BOARD_MODEM_DTR_PIN, int uart_rts_pin = -1, int uart_cts_pin = -1)
    {
        this->uart_rx_pin = uart_rx_pin;
        this->uart_tx_pin = uart_tx_pin;
        this->uart_rts_pin = uart_rts_pin;
        this->uart_cts_pin = uart_cts_pin;
        this->dtr_pin = dtr_pin;
    }

    ~ModemSim7670()
    {
        shutdown();
    }

    bool init()
    {
        /* Init and register system/core components */
        pinMode(BOARD_MODEM_PWR_PIN, OUTPUT);
        digitalWrite(BOARD_MODEM_PWR_PIN, HIGH);
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        ESP_ERROR_CHECK(esp_netif_init());
        dte_config.uart_config.tx_io_num = BOARD_MODEM_RXD_PIN;
        dte_config.uart_config.rx_io_num = BOARD_MODEM_TXD_PIN;
        dte_config.uart_config.rts_io_num = -1;
        dte_config.uart_config.cts_io_num = -1;
        dte_config.uart_config.flow_control = ESP_MODEM_FLOW_CONTROL_NONE;
        dte = create_uart_dte(&dte_config);
        assert(dte);
        esp_netif = esp_netif_new(&netif_ppp_config);
        assert(esp_netif);
        dce = create_SIM7670_GNSS_dce(&dce_config, dte, esp_netif);
        dce->set_dtr_pin(BOARD_MODEM_DTR_PIN);
        assert(dce);
        light_sleep(5);
        bool pin_ok = true;
        if (dce->read_pin(pin_ok) == command_result::OK)
        {
            if (!pin_ok && dce->set_pin("5461") != command_result::OK)
            {
                ESP_LOGE(TAG, "Cannot set PIN!");
                return false;
            }
        }
        std::string str;
        const uint32_t timeout = 2 * 60U * 1000U;
        auto now = millis();
        while (dce->get_operator_name(str) != esp_modem::command_result::OK && millis() - now < timeout)
        {
            // Getting operator name could fail... retry after 500 ms
            ESP_LOGW(TAG, " no operator name..");
            light_sleep(1);
        }
        if (dce->get_operator_name(str) != esp_modem::command_result::OK)
        {
            ESP_LOGE(TAG, "Cannot get operator name!");
            return false;
        }
        ESP_LOGI(TAG, " operator name %s", str.c_str());
        if (dce->get_imsi(str) == esp_modem::command_result::OK)
        {
            std::cout << "Modem IMSI number:" << str << std::endl;
        }
        initialized = true;
        return true;
    }

    std::unique_ptr<DCE_gnss> &GetDce()
    {
        return dce;
    }

    StatusHandler &GetEventHandler()
    {
        return handler;
    }

    bool EnableGnss(bool enable)
    {
        if (!dce || !initialized)
        {
            return false;
        }
        if (enable)
        {
            return dce->set_gnss_power_mode(false) == command_result::OK;
        }
        else
        {
            return dce->set_gnss_power_mode(true) == command_result::OK;
        }
        return false;
    }

    bool get_gnss(sim76xx_gps &gps)
    {
        if (dce->get_gnss_information_sim76xx(gps) == esp_modem::command_result::OK)
        {
            ESP_LOGI(TAG, "gps.run  %i",
                     gps.run);
            ESP_LOGI(TAG, "gps.fix  %i",
                     gps.fix);
            ESP_LOGI(TAG, "gps.date.year %i gps.date.month %i gps.date.day %i",
                     gps.date.year, gps.date.month, gps.date.day);
            ESP_LOGI(TAG, "gps.tim.hour %i gps.tim.minute %i   gps.tim.second %i   gps.tim.thousand %i",
                     gps.tim.hour, gps.tim.minute, gps.tim.second, gps.tim.thousand);
            ESP_LOGI(TAG, "gps.latitude %f gps.longitude %f ",
                     gps.latitude, gps.longitude);
            ESP_LOGI(TAG, "gps.altitude  %f",
                     gps.altitude);
            ESP_LOGI(TAG, "gps.speed  %f",
                     gps.speed);
            ESP_LOGI(TAG, "gps.cog  %f",
                     gps.cog);
            ESP_LOGI(TAG, "gps.fix_mode  %i",
                     gps.fix_mode);
            ESP_LOGI(TAG, "gps.dop_h %f gps.dop_p %f gps.dop_v %f ",
                     gps.dop_h, gps.dop_p, gps.dop_v);
            ESP_LOGI(TAG, "gps.sats_in_view  %i",
                     gps.sat.num);
            ESP_LOGI(TAG, "gps.hpa  %f gps.vpa  %f",
                     gps.hpa, gps.vpa);
            return true;
        }
        return false;
    }

    bool shutdown()
    {
        pinMode(BOARD_MODEM_PWR_PIN, OUTPUT);
        digitalWrite(BOARD_MODEM_PWR_PIN, LOW);
        dce.reset();
        esp_netif_destroy(esp_netif);
        dte.reset();
        initialized = false;
        return true;
    }

    bool setLowPwerMode(bool enable)
    {
        if (!dce || !initialized)
        {
            return false;
        }
        lowpPowerMode = enable;
        return dce->enable_sleep_mode(enable) == command_result::OK;
    }

    bool wakeupDTR(bool wake)
    {
        if (!dce || !initialized || !lowpPowerMode)
        {
            return false;
        }
        return dce->wake_via_dtr(wake) == command_result::OK;
    }

    bool sendSMS(const char *number, const char *message)
    {
        if (!dce || !initialized)
        {
            return false;
        }
        dce->sms_character_set();
        return dce->send_sms(number, message) == command_result::OK;
    }

private:
    /* Configure and create the DTE */
    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    std::shared_ptr<DTE> dte;
    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG("free");
    esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();
    bool initialized = false;
    bool gnss_enabled = false;
    bool lowpPowerMode = false;
    esp_netif_t *esp_netif = nullptr;
    std::unique_ptr<DCE_gnss> dce = nullptr;
    StatusHandler handler;
    int uart_rx_pin = -1;
    int uart_tx_pin = -1;
    int uart_rts_pin = -1;
    int uart_cts_pin = -1;
    int dtr_pin = -1;
};

void StartModem()
{
    ModemSim7670 modem;
    if (!modem.init())
    {
        return;
    }
    // dce->sms_character_set();
    // dce->send_sms("0758829590", "hi rami before CMUX_MODE");
    if (modem.GetDce()->set_mode(esp_modem::modem_mode::CMUX_MODE))
    {
        std::cout << "Modem has correctly entered multiplexed command/data mode" << std::endl;
        if (!modem.GetEventHandler().wait_for(StatusHandler::IP_Event, 60000))
        {
            ESP_LOGE(TAG, "Cannot get IP within specified timeout... exiting");
            return;
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to configure multiplexed command mode... exiting");
        modem.shutdown();
        return;
    }
    modem.sendSMS("0758829590", "hi rami after CMUX_MODE");

    if (modem.GetEventHandler().get_ip_event_type() == IP_EVENT_PPP_GOT_IP)
    {
        std::cout << "Got IP address" << std::endl;
        /* When connected to network, subscribe and publish some MQTT data */
        MqttClient mqtt;
        mqtt.connect();
        std::cout << "Connected" << std::endl;
        // dce->sms_txt_mode();
        // dce->sms_character_set();
        // dce->send_sms("0758829590", "hi rami");
    }
    else if (modem.GetEventHandler().get_ip_event_type() == IP_EVENT_PPP_LOST_IP)
    {
        ESP_LOGE(TAG, "PPP client has lost connection... exiting");
        return;
    }

    return;
}
