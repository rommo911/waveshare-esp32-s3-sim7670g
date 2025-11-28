#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "pins.hpp"
#include <vector>
#include <array>

class LedStrip {
public:
    enum class colors : uint32_t {
        Red = 0xFF0000,
        Green = 0x00FF00,
        Blue = 0x0000FF,
        Yellow = 0xFFFF00,
        Cyan = 0x00FFFF,
        Magenta = 0xFF00FF,
        White = 0xFFFFFF,
        Black = 0x000000,
        // some more colors
        Orange = 0xFFA500,
        Purple = 0x800080,
        Pink = 0xFFC0CB,
        Lime = 0x00FF00,
        Teal = 0x008080,
        Indigo = 0x4B0082,
        Violet = 0xEE82EE,
        Maroon = 0x800000,
        Olive = 0x808000,
        Navy = 0x000080,
        Aquamarine = 0x7FFFD4,
        Turquoise = 0x40E0D0,
        Silver = 0xC0C0C0,
        Gray = 0x808080,
        Gold = 0xFFD700
    };


    enum class Priority {
        NORMAL,
        NOTIFICATION,
        ALERT,
        COUNT // Helper to know the number of priorities
    };

    enum class Mode {
        OFF,
        SOLID,
        BLINK,
        FADE
    };

    // Constructor
    LedStrip(uint16_t num_pixels, int16_t pin = PIXEL_LED_PIN, neoPixelType type = NEO_GRB + NEO_KHZ800);
    ~LedStrip();

    // Initializes the strip and starts the worker task
    void begin();

    // Set a single LED to a solid color
    void setSolid(uint16_t pixel_index, uint32_t color, Priority priority = Priority::NORMAL);

    // Blink a single LED
    void setBlink(uint16_t pixel_index, uint32_t color, uint32_t to_color, uint16_t on_time_ms, uint16_t off_time_ms, uint16_t cycles = 0, uint32_t duration_ms = 0, Priority priority = Priority::NOTIFICATION);

    // Fade a single LED
    void setFade(uint16_t pixel_index, uint32_t from_color, uint32_t to_color, uint16_t cycle_time_ms, uint16_t cycles = 0, uint32_t duration_ms = 0, Priority priority = Priority::NOTIFICATION);

    // Clear the state for a given priority on a single LED
    void clear(uint16_t pixel_index, Priority priority);

    // Clear all states for a single LED
    void clear(uint16_t pixel_index);

    // Clear all states for all LEDs
    void clearAll();

    static uint32_t batteryColor(float percent);
    
    static inline uint32_t _rgb( uint8_t r, uint8_t g, uint8_t b)
    {
        return (uint32_t)((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }



private:
    struct LedState {
        Mode mode = Mode::OFF;
        Priority priority = Priority::NORMAL;
        uint32_t color_from = 0;
        uint32_t color_to = 0;
        uint16_t on_time_ms = 0;
        uint16_t off_time_ms = 0;
        uint16_t cycle_time_ms = 0;

        uint16_t total_cycles = 0;
        uint32_t duration_ms = 0;

        // Runtime variables
        unsigned long start_time_ms = 0;
        unsigned long last_step_ms = 0;
        uint16_t cycles_completed = 0;
        bool blink_on_state = false;
    };

    Adafruit_NeoPixel _strip;
    uint16_t _num_pixels;

    std::vector<std::array<LedState, static_cast<int>(Priority::COUNT)>> _pixel_states;
    
    TaskHandle_t _worker_task;
    EventGroupHandle_t _event_group;
    static const int STATE_CHANGED_BIT = BIT0;

    void _run_loop();
    static void _worker_trampoline(void* arg);

    static uint32_t lerp(uint32_t color1, uint32_t color2, float t);
};

extern LedStrip builtinLed;
extern LedStrip NotifyLed;