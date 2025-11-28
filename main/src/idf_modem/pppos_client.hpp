#ifndef _PPPOS_CLIENT_H_
#define _PPPOS_CLIENT_H_
// IMPRTANT order of include is important do not change
#include <Arduino.h>
#include <string.h>
#include <cstring>
#include <iostream>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "cxx_include/esp_modem_dte.hpp"
#include "esp_modem_config.h"
#include "cxx_include/esp_modem_api.hpp"
#include "cxx_include/esp_modem_api.hpp"
#include "SIM7670_gnss.hpp"
#include "pins.hpp"
// #include "main.hpp"

using namespace esp_modem;

class StatusHandler;

// using namespace esp_modem;

class ModemSim7670
{
public:
    ModemSim7670(int uart_rx_pin = BOARD_MODEM_RXD_PIN, int uart_tx_pin = BOARD_MODEM_TXD_PIN, int dtr_pin = BOARD_MODEM_DTR_PIN, int uart_rts_pin = -1, int uart_cts_pin = -1);

    ~ModemSim7670();
    
    typedef enum SleepMode
    {
        UNKNOWN = -1,
        ON = 0,
        TERMINAL_SLEEP,
        INTERRUPT_READY,
        SLEEP,
        OFF

    } SleepMode_t;

    bool init();
    void initAsync();

    std::unique_ptr<DCE_gnss> &GetDce();

    StatusHandler &GetEventHandler();

    bool EnableGnss(bool enable);

    bool get_gnss(sim76xx_gps_t &gps, uint16_t timeout_seconds);

    bool shutdown();

    bool wakeupDTR(bool wake);

    bool sendSMS(const char *number, const char *message);

    bool isInitialized();

    bool connectToInternet();

    bool disconnectInternet();

    bool SyncModemTimeToSystem(uint8_t timeout_seconds);

    bool sleep(SleepMode_t mode);

private:
    /* Configure and create the DTE */
    esp_modem_dte_config_t dte_config = {};
    esp_modem_dce_config_t dce_config = {};
    std::shared_ptr<DTE> dte;
    esp_netif_config_t netif_ppp_config = {};
    bool initialized = false;
    bool gnss_enabled = false;
    esp_netif_t *esp_netif = nullptr;
    std::unique_ptr<DCE_gnss> dce = nullptr;
    int uart_rx_pin = -1;
    int uart_tx_pin = -1;
    int uart_rts_pin = -1;
    int uart_cts_pin = -1;
    int dtr_pin = -1;
    const char *TAG = "modem-pppos";
};

extern ModemSim7670 modem;

#endif
