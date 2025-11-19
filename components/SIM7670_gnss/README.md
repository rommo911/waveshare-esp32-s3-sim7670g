# SIM7670 GNSS Component

This component provides a driver for the GNSS (Global Navigation Satellite System) functionality of the SIM7670 series modem on the ESP32-S3 platform. It is built upon the `esp-modem` library and extends its functionality to include GNSS data acquisition.

# SIM7670 additional features
This component also supports additional features of the modem, such as handling SMS messages (text mode, listing unread messages, deleting messages). Future additions may include power management, interrupt handling, and other modem-specific configurations.

## Analysis

The `SIM7670_gnss` component is designed as an extension to the existing `esp-modem` component, which provides a generic interface for controlling cellular modems. This component specializes the generic `SIM7600` class to create a `SIM7670_gnss` class with added GNSS capabilities.

### Architecture

- **`SIM7670_gnss.hpp`**: This header file defines the public interface of the component.
    - `SIM7670_gnss` class: Inherits from `esp_modem::SIM7600` and adds the `get_gnss_information_sim76xx` method.
    - `DCE_gnss` class: A Data Communication Equipment (DCE) class that wraps the `SIM7670_gnss` device, exposing its methods.
    - `create_SIM7670_GNSS_dce()`: A factory function to create instances of the `DCE_gnss` object.

- **`SIM7670_gnss.cpp`**: This source file contains the implementation.
    - It uses the `AT+CGNSINF` command to retrieve GNSS information from the modem.
    - It parses the response from the modem and populates the `sim76xx_gps_t` struct.
    - A local factory (`gnss_factory::LocalFactory`) is used to construct the specialized `DCE_gnss` object.

- **`sim76xx_gps.h`**: This header defines the data structures for holding GPS data, such as `sim76xx_gps_t`, `gps_fix_t`, `gps_time_t`, etc. These are plain C-style structs for maximum compatibility.

The component follows a clean, object-oriented approach by extending existing classes from the `esp-modem` library rather than modifying them directly. This makes the code modular and easier to maintain.

### How it Works

1.  The user calls `create_SIM7670_GNSS_dce()` to get a `DCE_gnss` object.
2.  To get GPS data, the user calls the `get_gnss_information_sim76xx()` method on the `DCE_gnss` object.
3.  This call is forwarded to the underlying `SIM7670_gnss` device.
4.  The device sends the `AT+CGNSINF` command to the modem via the DTE (Data Terminal Equipment) interface.
5.  The modem responds with a string containing the GNSS data.
6.  The `get_gnss_information_sim76xx_lib()` function parses this string and fills the user-provided `sim76xx_gps_t` struct with the parsed values (latitude, longitude, altitude, time, etc.).

## Usage

To use this component in your project, you need to:

1.  Include the component in your `CMakeLists.txt`.
2.  Include the `SIM7670_gnss.hpp` header in your source file.
3.  Create a DTE object and a `dce_config` struct as you would with the standard `esp-modem`.
4.  Call `create_SIM7670_GNSS_dce()` to create the DCE object.
5.  Create a `sim76xx_gps_t` variable to hold the GPS data.
6.  Call `dce->get_gnss_information_sim76xx(gps_data)` to retrieve the data.

```cpp
#include "SIM7670_gnss.hpp"

// ...

// 1. Setup DTE and event loop
// ... (standard esp-modem setup)

// 2. Create the DCE object
auto dce = create_SIM7670_GNSS_dce(&dce_config, dte, netif);

// 3. Get GNSS data
sim76xx_gps_t gps_data;
esp_modem::command_result res = dce->get_gnss_information_sim76xx(gps_data);

if (res == esp_modem::command_result::OK) {
    // Use gps_data
    printf("Latitude: %f, Longitude: %f\n", gps_data.latitude, gps_data.longitude);
}

// 4. Set SMS to text mode
dce->set_sms_text_mode(true);

// 5. Get unread SMS messages
std::list<sms_t> sms_list;
res = dce->get_unread_sms_list(sms_list);

if (res == esp_modem::command_result::OK) {
    for (const auto& sms : sms_list) {
        printf("SMS from %s: %s\n", sms.sender.c_str(), sms.content.c_str());
        // Optionally, delete the message after reading
        // dce->delete_sms(sms.index);
    }
}

// 6. Set auto-answer after 1 ring
if (dce->set_auto_answer(1) == esp_modem::command_result::OK) {
    printf("Auto-answer enabled.\n");
}

// 7. Set power mode to flight mode (RF off)
if (dce->set_functionality_level(functionality_level_t::FLIGHT_MODE) == esp_modem::command_result::OK) {
    printf("Modem in flight mode.\n");
}

// 8. Enable sleep mode
if (dce->enable_sleep_mode(true) == esp_modem::command_result::OK) {
    printf("Modem sleep mode enabled.\n");
    // The modem will now enter a low-power state when idle. It can be woken up via the DTR pin.
}

// 9. Configure the DTR pin (do this once at setup)
dce->set_dtr_pin(BOARD_MODEM_DTR_PIN);

// 10. Before sending a command when the modem might be asleep, wake it up
dce->wake_via_dtr();

// Now it's safe to send a command
if (dce->get_operator_name(str) == esp_modem::command_result::OK) {
    printf("Operator: %s\n", str.c_str());
}

// 11. Sync system time with the network time for a specific timezone
// Example for Paris (Central European Time with daylight saving)
if (dce->sync_system_time("CET-1CEST,M3.5.0,M10.5.0/3")) {
    printf("System time synced successfully.\n");
    time_t now;
    char strftime_buf[64];
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    printf("Current local time: %s\n", strftime_buf);
}

// 12. Configure modem for wake-on-SMS
// This setup is ideal for low-power applications that need to react to incoming messages.
// First, enable sleep mode on the modem.
dce->enable_sleep_mode(true);

// Second, configure the RI (Ring Indicator) pin to assert on SMS and other events.
dce->set_ring_indicator_mode(ring_indicator_mode_t::SMS_CALL_URC);

// Now, on the ESP32 side, you must configure the GPIO connected to the modem's RI pin
// (BOARD_MODEM_RI_PIN) as a wake-up source.
// Example:
// gpio_wakeup_enable(BOARD_MODEM_RI_PIN, GPIO_INTR_LOW_LEVEL);
// esp_sleep_enable_gpio_wakeup();
// esp_deep_sleep_start();
}
```

## Developing New Functionalities

This component is designed to be extensible. To add new functionality (e.g., support for another AT command):

1.  **Add a new method to `SIM7670_gnss` class** (in `SIM7670_gnss.hpp`):
    Define the new function that will represent the new AT command. For example:
    ```cpp
    class SIM7670_gnss: public esp_modem::SIM7600 {
    public:
        // ... existing methods
        esp_modem::command_result get_battery_status(int &voltage);
    };
    ```

2.  **Implement the method in `SIM7670_gnss.cpp`**:
    Create a library-style function that takes a `CommandableIf*` and sends the AT command. Then, implement the class method to call this function. This pattern keeps the AT command logic separate from the class itself.
    ```cpp
    // In SIM7670_gnss.cpp

    // Lib-style function
    esp_modem::command_result get_battery_status_lib(esp_modem::CommandableIf *t, int &voltage)
    {
        // Implementation to send AT command (e.g., AT+CBC) and parse the result
    }

    // Class method implementation
    esp_modem::command_result SIM7670_gnss::get_battery_status(int &voltage)
    {
        return get_battery_status_lib(dte.get(), voltage);
    }
    ```

3.  **Expose the method in `DCE_gnss`** (in `SIM7670_gnss.hpp`):
    Add the new method to the `DCE_gnss` class to forward the call to the underlying device.
    ```cpp
    class DCE_gnss : public esp_modem::DCE_T<SIM7670_gnss> {
    public:
        // ...
        esp_modem::command_result get_battery_status(int &voltage);
    };
    ```

4.  **Implement the forwarding in `SIM7670_gnss.cpp`**:
    ```cpp
    // In SIM7670_gnss.cpp
    esp_modem::command_result DCE_gnss::get_battery_status(int &voltage)
    {
        return device->get_battery_status(voltage);
    }
    ```
    Alternatively, if you are adding many commands, you can use the `ESP_MODEM_DECLARE_DCE_COMMAND` macro within the `DCE_gnss` class definition in the header for simplicity.

## Documentation Generation

This component is commented using Doxygen-style comments. To generate HTML documentation:

1.  Install Doxygen.
2.  Navigate to the `components/SIM7670_gnss` directory.
3.  Run the command: `doxygen Doxyfile`
4.  The generated documentation will be in the `docs/` subdirectory. Open `docs/html/index.html` in a web browser.

```