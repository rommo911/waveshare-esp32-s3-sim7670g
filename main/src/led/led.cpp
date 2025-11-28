#include "led.hpp"

// Constructor
LedStrip::LedStrip(uint16_t num_pixels, int16_t pin, neoPixelType type)
    : _strip(num_pixels, pin, type),
      _num_pixels(num_pixels),
      _worker_task(NULL),
      _event_group(NULL) {
    _pixel_states.resize(num_pixels);
}

// Destructor
LedStrip::~LedStrip() {
    if (_worker_task != NULL) {
        vTaskDelete(_worker_task);
    }
    if (_event_group != NULL) {
        vEventGroupDelete(_event_group);
    }
}

void LedStrip::begin() {
    _strip.begin();
    _strip.setBrightness(255);
    _strip.clear();
    _strip.show(); // Initialize all pixels to 'off'
    _event_group = xEventGroupCreate();
    if (_event_group == NULL) {
        // Handle error: event group creation failed
        return;
    }
    
    xTaskCreate(
        _worker_trampoline,
        "led_worker",
        2048, // Increased stack size for vector/array operations
        this,   // Task parameter
        1,      // Priority
        &_worker_task);
}

void LedStrip::setSolid(uint16_t pixel_index, uint32_t color, Priority priority) {
    if (pixel_index >= _num_pixels) return;
    int p_idx = static_cast<int>(priority);
    _pixel_states[pixel_index][p_idx] = {
        .mode = Mode::SOLID,
        .priority = priority,
        .color_from = color,
        .start_time_ms = millis()
    };
    if (_event_group) xEventGroupSetBits(_event_group, STATE_CHANGED_BIT);
}

void LedStrip::setBlink(uint16_t pixel_index, uint32_t color, uint32_t to_color, uint16_t on_time_ms, uint16_t off_time_ms, uint16_t cycles, uint32_t duration_ms, Priority priority) {
    if (pixel_index >= _num_pixels) return;
    int p_idx = static_cast<int>(priority);
    _pixel_states[pixel_index][p_idx] = {
        .mode = Mode::BLINK,
        .priority = priority,
        .color_from = color,
        .color_to = to_color,
        .on_time_ms = on_time_ms,
        .off_time_ms = off_time_ms,
        .total_cycles = cycles,
        .duration_ms = duration_ms,
        .start_time_ms = millis(),
        .last_step_ms = millis(),
        .blink_on_state = true // Start with LED on
    };
    if (_event_group) xEventGroupSetBits(_event_group, STATE_CHANGED_BIT);
}

void LedStrip::setFade(uint16_t pixel_index, uint32_t from_color, uint32_t to_color, uint16_t cycle_time_ms, uint16_t cycles, uint32_t duration_ms, Priority priority) {
    if (pixel_index >= _num_pixels) return;
    int p_idx = static_cast<int>(priority);
    _pixel_states[pixel_index][p_idx] = {
        .mode = Mode::FADE,
        .priority = priority,
        .color_from = from_color,
        .color_to = to_color,
        .cycle_time_ms = cycle_time_ms,
        .total_cycles = cycles,
        .duration_ms = duration_ms,
        .start_time_ms = millis()
    };
    if (_event_group) xEventGroupSetBits(_event_group, STATE_CHANGED_BIT);
}

void LedStrip::clear(uint16_t pixel_index, Priority priority) {
    if (pixel_index >= _num_pixels) return;
    _pixel_states[pixel_index][static_cast<int>(priority)].mode = Mode::OFF;
    if (_event_group) xEventGroupSetBits(_event_group, STATE_CHANGED_BIT);
}

void LedStrip::clear(uint16_t pixel_index) {
    if (pixel_index >= _num_pixels) return;
    for (int i = 0; i < static_cast<int>(Priority::COUNT); ++i) {
        _pixel_states[pixel_index][i].mode = Mode::OFF;
    }
    if (_event_group) xEventGroupSetBits(_event_group, STATE_CHANGED_BIT);
}

void LedStrip::clearAll() {
    for (uint16_t i = 0; i < _num_pixels; ++i) {
        for (int j = 0; j < static_cast<int>(Priority::COUNT); ++j) {
            _pixel_states[i][j].mode = Mode::OFF;
        }
    }
    if (_event_group) xEventGroupSetBits(_event_group, STATE_CHANGED_BIT);
}

void LedStrip::_worker_trampoline(void* arg) {
    LedStrip* instance = static_cast<LedStrip*>(arg);
    instance->_run_loop();
}

void LedStrip::_run_loop() {
    const TickType_t wait_ticks = pdMS_TO_TICKS(20); // Loop at least every 20ms for smooth animations

    while (true) {
        xEventGroupWaitBits(_event_group, STATE_CHANGED_BIT, pdTRUE, pdFALSE, wait_ticks);

        unsigned long now = millis();
        bool needs_show = false;

        for (uint16_t i = 0; i < _num_pixels; ++i) {
            // 1. Find the active state for this pixel
            LedState* active_state = nullptr;
            for (int p = static_cast<int>(Priority::COUNT) - 1; p >= 0; --p) {
                LedState& state = _pixel_states[i][p];
                if (state.mode == Mode::OFF) {
                    continue;
                }

                bool expired = false;
                if (state.duration_ms > 0 && (now - state.start_time_ms >= state.duration_ms)) expired = true;
                if (state.total_cycles > 0 && state.cycles_completed >= state.total_cycles) expired = true;

                if (expired) {
                    state.mode = Mode::OFF;
                    continue;
                }
                
                active_state = &state;
                break;
            }

            // 2. Calculate the color based on the active state
            uint32_t new_color = 0;
            if (active_state) {
                 switch (active_state->mode) {
                    case Mode::SOLID:
                        new_color = active_state->color_from;
                        break;
                    case Mode::BLINK: {
                        uint16_t step_duration = active_state->blink_on_state ? active_state->on_time_ms : active_state->off_time_ms;
                        if (now - active_state->last_step_ms >= step_duration) {
                            active_state->blink_on_state = !active_state->blink_on_state;
                            active_state->last_step_ms = now;
                            if (active_state->blink_on_state) active_state->cycles_completed++;
                        }
                        new_color = active_state->blink_on_state ? active_state->color_from : 0;
                        break;
                    }
                    case Mode::FADE: {
                        if (active_state->cycle_time_ms > 0) {
                            unsigned long elapsed = now - active_state->start_time_ms;
                            unsigned long cycle_pos = elapsed % active_state->cycle_time_ms;
                            float t = (float)cycle_pos / active_state->cycle_time_ms;
                            float progress = 1.0f - fabsf(2.0f * t - 1.0f); // Triangle wave 0->1->0
                            new_color = lerp(active_state->color_from, active_state->color_to, progress);
                            
                            if (elapsed / active_state->cycle_time_ms >= active_state->cycles_completed) {
                                active_state->cycles_completed = elapsed / active_state->cycle_time_ms;
                            }
                        } else {
                            new_color = active_state->color_from;
                        }
                        break;
                    }
                    default: break;
                }
            }

            // 3. Apply the color if it has changed
            if (_strip.getPixelColor(i) != new_color) {
                _strip.setPixelColor(i, new_color);
                needs_show = true;
            }
        }

        if (needs_show) {
            _strip.show();
        }
    }
}

uint32_t LedStrip::lerp(uint32_t color1, uint32_t color2, float t) {
    uint8_t r1 = (color1 >> 16) & 0xFF, g1 = (color1 >> 8) & 0xFF, b1 = color1 & 0xFF;
    uint8_t r2 = (color2 >> 16) & 0xFF, g2 = (color2 >> 8) & 0xFF, b2 = color2 & 0xFF;
    uint8_t r = r1 + (r2 - r1) * t;
    uint8_t g = g1 + (g2 - g1) * t;
    uint8_t b = b1 + (b2 - b1) * t;
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}
uint32_t LedStrip::batteryColor(float percent) {
  if (percent >= 90) {
    return _rgb(0, 50, 0);
  }
  if (percent <= 10) {
    return _rgb(50, 0, 0);
  }
  // map both red and green to 0..50 and make them complementary so
  // r + g == 50 (max). This keeps a smooth red->green gradient while
  // ensuring the sum never exceeds 50.
  uint8_t r = (uint8_t)(((uint16_t)(100 - percent) * 50) / 100);
  uint8_t g = (uint8_t)(((uint16_t)percent * 50) / 100);
  return _rgb(r, g, 0);
}


LedStrip builtinLed(1, PIXEL_LED_PIN, NEO_RGB + NEO_KHZ800);
LedStrip NotifyLed(2, NOTIFY_PIXEL_LED_PIN, NEO_GRB + NEO_KHZ800);
