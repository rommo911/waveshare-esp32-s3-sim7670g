#include "carBattery.hpp"

CarBattery::CarBattery() : lowBatteryThreshold(11.8f) { // Default value
}

void CarBattery::begin() {
    // Initialize GPIO pins
    pinMode(ADC_PIN, INPUT);
    pinMode(ENGINE_SENSE_PIN, INPUT_PULLDOWN); // Assuming the pin will be pulled HIGH when engine is on

    // Initialize Preferences
    preferences.begin(preferences_namespace, false); // false for read/write

    // Load the low battery threshold from NVS
    // If the key doesn't exist, it will use the default value provided (11.8f)
    lowBatteryThreshold = preferences.getFloat(nvs_key_low_batt, 11.9f);
    preferences.end();
}

float CarBattery::getBatteryVoltage() {
    // Read the ADC value
    int adcValue = analogRead(ADC_PIN);

    // Convert ADC value to voltage at the pin
    float pinVoltage = (adcValue / (float)ADC_MAX_VALUE) * ADC_REF_VOLTAGE;

    // Convert pin voltage to actual battery voltage using the divider ratio
    // It is better to use analogReadMilliVolts() for more accurate readings if available and calibrated
    // For now, we use the basic analogRead and a theoretical ratio
    float batteryVoltage = pinVoltage * VOLTAGE_DIVIDER_RATIO;

    return batteryVoltage;
}

bool CarBattery::isEngineOn() {
    // Read the digital GPIO pin
    // We assume that the signal is HIGH when the engine is running
    return digitalRead(ENGINE_SENSE_PIN) == HIGH;
}

bool CarBattery::isBatteryLow() {
    return getBatteryVoltage() < lowBatteryThreshold;
}

void CarBattery::setLowBatteryThreshold(float voltage) {
    lowBatteryThreshold = voltage;
    preferences.begin(preferences_namespace, false);
    preferences.putFloat(nvs_key_low_batt, voltage);
    preferences.end();
}

float CarBattery::getLowBatteryThreshold() {
    preferences.begin(preferences_namespace, true); // true for read-only
    float threshold = preferences.getFloat(nvs_key_low_batt, 11.8f);
    preferences.end();
    return threshold;
}
