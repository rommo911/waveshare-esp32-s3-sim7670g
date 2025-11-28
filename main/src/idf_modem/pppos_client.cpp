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
#include "pppos_client.hpp"
#include "esp_log.h"
#include "esp_event.h"
#include "power/power.hpp"
#include <sys/time.h>
#include "driver/rtc_io.h"
#include <thread>

using namespace esp_modem;
std::thread init_async_thread;
static ModemSim7670::SleepMode_t RTC_DATA_ATTR SleepMODE = ModemSim7670::SleepMode_t::UNKNOWN;

class StatusHandler
{
public:
    static constexpr auto IP_Event = SignalGroup::bit0;

    StatusHandler()
    {
    }

    ~StatusHandler()
    {
        esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID, on_event);
        isinit = false;
    }
    void begin()
    {
        if (isinit)
            return;
        esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, on_event, this);
        isinit = true;
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
    bool isinit = false;
    const char *TAG = "IP-event";
};

StatusHandler handler;

ModemSim7670::ModemSim7670(int uart_rx_pin, int uart_tx_pin, int dtr_pin, int uart_rts_pin, int uart_cts_pin)
{
    this->uart_rx_pin = uart_rx_pin;
    this->uart_tx_pin = uart_tx_pin;
    this->uart_rts_pin = uart_rts_pin;
    this->uart_cts_pin = uart_cts_pin;
    this->dtr_pin = dtr_pin;
}

ModemSim7670::~ModemSim7670()
{
    shutdown();
}

void ModemSim7670::initAsync()
{
    init_async_thread = std::thread([this]()
                                    { this->init(); });
}

bool ModemSim7670::init()
{
    if (initialized)
    {
        return true;
    }
    /* Init and register system/core components */
    pinMode(BOARD_MODEM_PWR_PIN, OUTPUT);
    digitalWrite(BOARD_MODEM_PWR_PIN, HIGH);
    pinMode(BOARD_MODEM_RI_PIN, INPUT_PULLUP);
    auto now = millis();
    esp_event_loop_create_default();
    esp_netif_init();
    dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    dte_config.uart_config.tx_io_num = BOARD_MODEM_RXD_PIN;
    dte_config.uart_config.rx_io_num = BOARD_MODEM_TXD_PIN;
    dte_config.uart_config.rts_io_num = -1;
    dte_config.uart_config.cts_io_num = -1;
    dte_config.uart_config.flow_control = ESP_MODEM_FLOW_CONTROL_NONE;
    dte = create_uart_dte(&dte_config);
    assert(dte);
    netif_ppp_config = ESP_NETIF_DEFAULT_PPP();
    esp_netif = esp_netif_new(&netif_ppp_config);
    assert(esp_netif);
    dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG("free");
    dce = create_SIM7670_GNSS_dce(&dce_config, dte, esp_netif);
    assert(dce);
    dce->set_dtr_pin(BOARD_MODEM_DTR_PIN);
    delay(100);
    ESP_LOGI(TAG, "modem init, mode=%d", SleepMODE);
    if ((SleepMODE == OFF || SleepMODE == UNKNOWN) && dce->sync() != command_result::OK)
    {
        ESP_LOGI(TAG, "modem init wait ..");
        delay(5000);
    }
    if (SleepMODE == SLEEP || SleepMODE == INTERRUPT_READY)
    {
        ESP_LOGI(TAG, "modem init wake DTR ..");
        dce->wake_via_dtr(true);
        dce->enable_terminal_sleep_mode(false);
    }
    uint32_t timeout = 7 * 1000U;
    SleepMODE = ON;
    ESP_LOGI(TAG, "modem waitin for terminal ..");
    while (dce->sync() != command_result::OK && millis() - now < timeout)
    {
        delay(200);
    }
    if (dce->sync() != command_result::OK)
    {
        ESP_LOGE(TAG, "Cannot sync modem");
        return false;
    }
    ESP_LOGI(TAG, "modem terminal ready.");
    auto end = millis();
    ESP_LOGI(TAG, "modem on after %d ms", end - now);
    bool pin_ok = true;
    if (dce->read_pin(pin_ok) == command_result::OK)
    {
        if (!pin_ok && dce->set_pin("5461") != command_result::OK)
        {
            ESP_LOGE(TAG, "Cannot set PIN!");
            sleep(SleepMode_t::SLEEP);
            return false;
        }
    }
    dce->set_functionality_level(functionality_level_t::FULL);
    std::string str;
    dce->at("AT+CPMVT=0\r", str, 4000); // set NO LOW VOLTAGE // something not OK ( battery pin ?)
    str = "";
    now = millis();
    timeout = 60U * 1000U;
    ESP_LOGI(TAG, "modem waitin network ..");
    esp_modem::command_result res = esp_modem::command_result::FAIL;
    while (res != esp_modem::command_result::OK && millis() - now < timeout)
    {
        res = dce->at("AT+COPS?", str, 1000);
        ESP_LOGI(TAG, "ret = %d , str = %s", res, str.c_str());
        if (res == esp_modem::command_result::FAIL)
        {
            delay(1000);
        }
    }
    while (dce->get_operator_name(str) != esp_modem::command_result::OK && millis() - now < timeout)
    {
        delay(1000);
    }
    if (dce->get_operator_name(str) != esp_modem::command_result::OK)
    {
        ESP_LOGE(TAG, "Cannot get operator name!");
        sleep(SleepMode_t::SLEEP);
        return false;
    }
    end = millis();
    ESP_LOGI(TAG, " operator name %s connected in %d ms", str.c_str(), end - now);
    str = "";
    res = esp_modem::command_result::FAIL;
    if (dce->get_imsi(str) == esp_modem::command_result::OK)
    {
        std::cout << "Modem IMSI number:" << str << std::endl;
    }
    dce->set_auto_answer(2);
    res = esp_modem::command_result::FAIL;
    initialized = true;
    return true;
}

bool ModemSim7670::SyncModemTimeToSystem(uint8_t timeout_seconds)
{
    if (!initialized)
    {
        return false;
    }
    struct tm timeinfo;
    esp_modem::command_result res = esp_modem::command_result::FAIL;
    auto now = millis();
    while (res != esp_modem::command_result::OK && ((millis() - now) < (timeout_seconds * 1000)))
    {
        res = dce->get_network_time(timeinfo);
        delay(1000);
    }
    if (res != esp_modem::command_result::OK)
    {
        return false;
    }
    struct timeval tv;
    tv.tv_sec = mktime(&timeinfo);
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
    // LOG TIME
    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time is: %s", strftime_buf);
    return false;
}

std::unique_ptr<DCE_gnss> &ModemSim7670::GetDce()
{
    return dce;
}

StatusHandler &ModemSim7670::GetEventHandler()
{
    return handler;
}

bool ModemSim7670::EnableGnss(bool enable)
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

bool ModemSim7670::get_gnss(sim76xx_gps_t &gps, uint16_t timeout_seconds)
{
    auto now = millis();
    while (dce->get_gnss_information_sim76xx(gps) != esp_modem::command_result::OK && ((millis() - now) < (timeout_seconds * 1000)))
    {
        delay(1000);
    }
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

bool ModemSim7670::shutdown()
{
    if (!initialized)
    {
        return false;
    }
    dce.reset();
    if (esp_netif)
    {
        esp_netif_destroy(esp_netif);
    }
    dte.reset();
    pinMode(BOARD_MODEM_PWR_PIN, OUTPUT);
    digitalWrite(BOARD_MODEM_PWR_PIN, LOW);
    Serial.println("modem shutdown");
    initialized = false;
    return true;
}

bool ModemSim7670::sleep(SleepMode_t mode)
{
    if (!dce)
    {
        return false;
    }
    // pinMode(BOARD_MODEM_PWR_PIN, OUTPUT);
    // digitalWrite(BOARD_MODEM_PWR_PIN, HIGH);
    SleepMODE = mode;
    EnableGnss(false);
    switch (mode)
    {
    case SLEEP:
    {
        dce->enable_terminal_sleep_mode(true);
        dce->wake_via_dtr(false);
        dce->set_functionality_level(functionality_level_t::MINIMUM);
        initialized = false;
        ESP_LOGI(TAG, "modem sleep");
        break;
    }
    case INTERRUPT_READY:
    {
        dce->set_ring_indicator_mode(ring_indicator_mode_t::SMS_CALL_ONLY);
        dce->enable_terminal_sleep_mode(true);
        dce->wake_via_dtr(false);
        gpio_wakeup_enable((gpio_num_t)BOARD_MODEM_RI_PIN, GPIO_INTR_LOW_LEVEL);
        esp_sleep_enable_gpio_wakeup();
        initialized = false;
        ESP_LOGI(TAG, "modem sleep INTERRUPT_READY");
        break;
    }
    case TERMINAL_SLEEP:
    {
        dce->enable_terminal_sleep_mode(true);
        dce->wake_via_dtr(false);
        ESP_LOGI(TAG, "modem sleep TERMINAL_SLEEP");

        break;
    }

    default:
        break;
    }
    return true;
}

bool ModemSim7670::sendSMS(const char *number, const char *message)
{
    if (!dce || !initialized)
    {
        return false;
    }
    auto ret = dce->sms_character_set();
    {
        if (ret == command_result::OK)
        {
            ret = dce->send_sms(number, message);
            if (ret == command_result::OK)
            {
                ESP_LOGI(TAG, "SMS sent to: %s , content: %s", number, message);
                return true;
            }
        }
    }
    ESP_LOGE(TAG, "SMS send to: %d Failed", number);
    return false;
}

bool ModemSim7670::isInitialized()
{
    return initialized;
}

bool ModemSim7670::connectToInternet()
{
    if (initialized)
    {
        GetEventHandler().begin();
        if (dce->set_mode(esp_modem::modem_mode::CMUX_MODE))
        {
            ESP_LOGI(TAG, "Modem has correctly entered multiplexed command/data mode");
            if (GetEventHandler().wait_for(StatusHandler::IP_Event, 60000))
            {
                ESP_LOGE(TAG, "Cannot get IP within specified timeout... exiting");
            }
            ESP_LOGI(TAG, "Modem hs connected");
            return true;
        }
        else
        {
            ESP_LOGE(TAG, "Failed to configure multiplexed command mode... exiting");
            //sleep(SleepMode_t::SLEEP);
            return false;
        }
    }
    return false;
}

bool ModemSim7670::disconnectInternet()
{
    if (initialized)
    {
        if (dce->set_mode(esp_modem::modem_mode::COMMAND_MODE))
        {
            std::cout << "Modem has correctly entered command mode" << std::endl;
            return true;
        }
        else
        {
            ESP_LOGE(TAG, "Failed to configure command mode");
            return false;
        }
    }
    return false;
}

ModemSim7670 modem;

//    modem.sendSMS("0758829590", "hi rami after CMUX_MODE");
