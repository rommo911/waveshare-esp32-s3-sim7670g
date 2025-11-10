/**
 * @file      modem7670g.cpp
 * @author    Lewis He (lewishe@outlook.com)
 * @license   MIT
 * @copyright Copyright (c) 2022  Shenzhen Xin Yuan Electronic Technology Co., Ltd
 * @date      2022-09-21
 *
 */
// See all AT commands, if wanted
// #define DUMP_AT_COMMANDS

#include "pins.hpp"
#include "modem/modem.hpp"
#include "power/power.hpp"

namespace modem
{

    const char *register_info[] = {
        "Not registered, MT is not currently searching an operator to register to. The GPRS service is disabled, the UE is allowed to attach for GPRS if requested by the user.",
        "Registered, home network.",
        "Not registered, but MT is currently trying to attach or searching an operator to register to. The GPRS service is enabled, but an allowable PLMN is currently not available. The UE will start a GPRS attach as soon as an allowable PLMN is available.",
        "Registration denied, the GPRS service is disabled, the UE is not allowed to attach for GPRS if it is requested by the user.",
        "Unknown.",
        "Registered, roaming.",
    };

    enum
    {
        MODEM_CATM = 1,
        MODEM_NB_IOT,
        MODEM_CATM_NBIOT,
    };
    // Your GPRS credentials, if any
    const char apn[] = "YourAPN";
    const char gprsUser[] = "";
    const char gprsPass[] = "";
    void GPSlocationTask(void *);
    bool getLocation();

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
    StreamDebugger debugger(Serial1, Serial);
    TinyGsm modem7600g(debugger);
#else
    TinyGsm modem7600g(Serial1);
#endif
    //TinyGsmClientSecure secureClient(modem7670g., 0);
    bool gps_enabled = false;
    bool gprs_enabled = false;
    bool modem_enabled = false;

    const char *SaraR5RegStatusERRORtoChar(SIM7600RegStatus status)
    {
        switch (status)
        {
        case REG_NO_RESULT:
            // 0x00
            return "No result";
        case REG_UNREGISTERED:
            // 0x01
            return "Unregistered";
        case REG_SEARCHING:
            // 0x02
            return "Searching";
        case REG_DENIED:
            // 0x03
            return "Denied";
        case REG_OK_HOME:
            // 0x04
            return "Home";
        case REG_OK_ROAMING:
            // 0x05
            return "Roaming";
        case REG_UNKNOWN:
        default:
            return "Unknown";
        }
    }

    bool SetGPS(bool enable)
    {
        if (!modem_enabled)
        {
            Serial.println("modem not started");
            return false;
        }
        if (enable && !gps_enabled)
        {
            // Modem GPS Power
            // power::getPMU().setBLDO2Voltage(3300);
            // power::getPMU().enableBLDO2();
            // delay(1000);
            Serial.println("Enabling GPS...");
            if (SetGPRS(false) == false)
            {
                Serial.println("GPRS disconnect failed!");
            }
            bool ret = modem7670g.enableGPS();
            if (ret)
            {
                modem7670g.sendAT(GF("+CGNSMOD=1,1,0,0,0"));
                if (ret == false)
                {
                    Serial.println("GPS failed set glonass mode..");
                }
                gps_enabled = true;
                Serial.println("GPS enable OK..starting task");
                xTaskCreate(GPSlocationTask, "gps", 4096, NULL, 1, NULL);
                return true;
            }
            else
            {
                Serial.println("GPS enable failed..");
                return false;
            }
        }
        else
        {
            if (gps_enabled)
            {
                Serial.println("Disable GPS...");
                // power::getPMU().disableBLDO2(); // Enable GPS power
                gps_enabled = false;
                return modem7670g.disableGPS();
            }
            return true;
        }
        return false;
    }

    bool SetGPRS(bool enable)
    {
        if (!modem_enabled)
        {
            Serial.println("modem not started");
            return false;
        }
        if (enable)
        {
            if (SetGPS(false) == false)
            {
                Serial.println("Disable GPS failed!");
            }
            Serial.println("Enable GPRS...");
            if (modem7670g.gprsConnect(apn, gprsUser, gprsPass) == false)
            {
                Serial.println("GPRS connect failed!");
                return false;
            }
            Serial.println("............................................................................Step 7");
            Serial.println("show the information of the Modem, SIM and network  ");

            Serial.println("T-SIM7080G Firmware Version: ");
            modem7670g.sendAT("+CGMR");
            String response;
            if (modem7670g.waitResponse(10000L, response) != 1)
            {
                Serial.println("Get Firmware Version Failed!");
            }
            else
            {
                Serial.println(response);
            }
            String ccid = modem7670g.getSimCCID();
            Serial.print("CCID:");
            Serial.println(ccid);

            String imei = modem7670g.getIMEI();
            Serial.print("IMEI:");
            Serial.println(imei);

            String imsi = modem7670g.getIMSI();
            Serial.print("IMSI:");
            Serial.println(imsi);

            String cop = modem7670g.getOperator();
            Serial.print("Operator:");
            Serial.println(cop);

            IPAddress local = modem7670g.localIP();
            Serial.print("Local IP:");
            Serial.println(local);

            int csq = modem7670g.getSignalQuality();
            Serial.print("Signal quality:");
            Serial.println(csq);

            modem7670g.sendAT("+CGNAPN");
            if (modem7670g.waitResponse(10000L) != 1)
            {
                Serial.println("Get APN Failed!");
                return false;
            }

            modem7670g.sendAT("+CCLK?");
            if (modem7670g.waitResponse(10000L) != 1)
            {
                Serial.println("Get time Failed!");
                return false;
            }
            gprs_enabled = true;
            return true;
        }
        else
        {
            if (gprs_enabled)
            {
                Serial.println("Disable GPRS...");
                if (modem7670g.gprsDisconnect() == false)
                {
                    Serial.println("GPRS disconnect failed!");
                }
            }
            gprs_enabled = false;
            return true;
        }
    }

    bool setupSim(const char *pin)
    {
        if (!modem_enabled)
        {
            Serial.println("modem not started");
            return false;
        }
        auto simStatus = modem7670g.getSimStatus();
        switch (simStatus)
        {
        case SIM_READY:
        {
            Serial.println("SIM Card is ready!");
            return false;
        }
        case SIM_LOCKED:
        {
            Serial.println("SIM Card is locked ...");
            Serial.println("trying to unlock ...");
            Serial.print("SIM PIN: ");
            Serial.println(pin);
            modem7670g.simUnlock(pin); // Replace with your SIM PIN
            delay(1000);
            simStatus = modem7670g.getSimStatus();
            if (simStatus == SIM_READY)
            {
                Serial.println("SIM Card is unlocked OK");
            }
            else
            {
                Serial.println("SIM Card unlock failed!");
                return false;
            }
            break;
        }
        case SIM_ANTITHEFT_LOCKED:
        {
            Serial.println("SIM Card is anti-theft locked ...");
            return false;
            break;
        }
        case SIM_ERROR:
        default:
        {
            Serial.println("SIM Card error ...");
            break;
        }
        }
        if (modem7670g.getSimStatus() != SIM_READY)
        {
            Serial.println("SIM Card is not inserted!!!");
            return false;
        }
        return true;
    }

    bool setRF(bool enable)
    {
        if (!modem_enabled)
        {
            Serial.println("modem not started");
            return false;
        }
        // Disable RF
        if (!enable)
        {
            modem7670g.sendAT("+CFUN=0");
            if (modem7670g.waitResponse(2000UL) != 1)
            {
                Serial.println("Disable RF Failed!");
                return false;
            }
        }
        else
        {
            // Enable RF
            modem7670g.sendAT("+CFUN=1");
            if (modem7670g.waitResponse(2000UL) != 1)
            {
                Serial.println("Enable RF Failed!");
                return false;
            }
        }
        return true;
    }

    bool initModem7080()
    {
        Serial.println("Initializing modem7670g...");
        // Modem 2700~3400mV VDD
        // power::getPMU().setDC3Voltage(3200);
        // power::getPMU().enableDC3();
        Serial1.begin(115200, SERIAL_8N1, BOARD_MODEM_RXD_PIN, BOARD_MODEM_TXD_PIN);
        pinMode(BOARD_MODEM_PWR_PIN, OUTPUT);
        /*digitalWrite(BOARD_MODEM_PWR_PIN, LOW);
        delay(100);
        digitalWrite(BOARD_MODEM_PWR_PIN, HIGH);
        delay(1000);
        digitalWrite(BOARD_MODEM_PWR_PIN, LOW);*/

        pinMode(BOARD_MODEM_DTR_PIN, OUTPUT);
        pinMode(BOARD_MODEM_RI_PIN, INPUT);
        int failCount = 0;
        const int retryCount = 10;
        int retry = 0;
        while (!modem7670g.testAT(1000) && failCount++ < (retryCount + 10))
        {
            Serial.print(".");
            if (retry++ > retryCount)
            {
                Serial.println("Warn : try reinit modem7670g.!");
                // Pull down PWRKEY for more than 1 second according to manual requirements
                digitalWrite(BOARD_MODEM_PWR_PIN, LOW);
                delay(100);
                digitalWrite(BOARD_MODEM_PWR_PIN, HIGH);
                delay(1000);
                digitalWrite(BOARD_MODEM_PWR_PIN, LOW);
                modem7670g.sendAT("+CRESET");
                retry = 0;
            }
        }
        if (modem7670g.testAT(1000))
        {

            Serial.println("Modem started!");
            modem7670g.sendAT("+CRESET");
            modem_enabled = true;
            return true;
        }
        return false;
    }

    bool startModem()
    {
        if (!modem_enabled)
        {
            Serial.println("modem not started");
            return false;
        }
        setupSim("5461");
        setRF(true);
        modem7670g.setNetworkMode(2); // use automatic

        SIM7600RegStatus status;
        uint16_t counter = 0;
        do
        {
            status = modem7670g.getRegistrationStatus();
            int16_t sq = modem7670g.getSignalQuality();

            if (status == REG_SEARCHING)
            {
                Serial.print("Searching...");
            }
            else
            {
                Serial.print("Other code:");
                Serial.println(SaraR5RegStatusERRORtoChar(status));
                break;
            }
            Serial.print(" Signal:");
            Serial.println(sq);
            delay(1000);
        } while (status != REG_OK_HOME && counter++ < 20);

        if (status == REG_OK_HOME)
        {
            Serial.println();
            Serial.print("Network register info:");
            if (status >= sizeof(register_info) / sizeof(*register_info))
            {
                Serial.print("Other result = ");
                Serial.println(status);
            }
            else
            {
                Serial.println(register_info[status]);
            }
            return true;
        }
        else
        {
            Serial.print("failed to register HOME network ");
            return false;
        }
        return false;
    }

    void GPSlocationTask(void *)
    {
        Serial.println("Get location");
        while (gps_enabled)
        {
            getLocation();
            delay(2000);
        }
        vTaskDelete(NULL);
    }

    bool shutdownModem()
    {
        if (modem_enabled)
        {
            SetGPRS(false);
            SetGPS(false);
            modem7670g.sendAT("+CRESET");
            Serial1.end();
        }
        // power::getPMU().disableDC3();   // disable modem power
        // power::getPMU().disableBLDO2(); // disable GPS power
        return true;
    }

} // namespace modem7670g.