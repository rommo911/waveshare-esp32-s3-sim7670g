#ifndef CAR_BATTERY_HPP
#define CAR_BATTERY_HPP

#include <Arduino.h>
#include <Preferences.h>
#include "pins.hpp"

class CarBattery {
public:
    CarBattery();
    void begin();
    float getBatteryVoltage();
    bool isEngineOn();
    bool isBatteryLow();
    void setLowBatteryThreshold(float voltage);
    float getLowBatteryThreshold();

private:
    // Hardcoded GPIO pins
    static const int ADC_PIN = 1;       // GPIO 1 for battery voltage ADC
    static const int ENGINE_SENSE_PIN = VBUS_INPUT_PIN; // GPIO 2 for engine running status

    // Voltage divider calibration.
    // Assumes a voltage divider brings the car battery voltage (~14V) down to the ESP32's ADC range (~3.3V).
    // Vin = Vout * (R1 + R2) / R2
    // This ratio needs to be calibrated based on the actual resistors used.
    // For example, with R1=100k and R2=27k, the ratio is (100k + 27k) / 27k = 4.703
    static constexpr float VOLTAGE_DIVIDER_RATIO = 4.703f;

    // ADC characteristics
    static const int ADC_MAX_VALUE = 4095;    // 12-bit ADC
    static constexpr float ADC_REF_VOLTAGE = 3.3f; // ADC reference voltage

    float lowBatteryThreshold;

    Preferences preferences;
    const char* preferences_namespace = "car_battery";
    const char* nvs_key_low_batt = "low_batt_v";
};

#endif // CAR_BATTERY_HPP
