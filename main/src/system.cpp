#include "system.hpp"
#include "esp_ota_ops.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "wifi/wifi.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

bool OTA_NEED_VALIDATION = false;
uint32_t OTA_VALID_FLAG = 0b1;
uint32_t OTA_INVALID_FLAG = 0b10;
EventGroupHandle_t ot_validation_event_group;

void set_ota_valid(bool valid)
{
    if (!ot_validation_event_group)
        return;
    if (!OTA_NEED_VALIDATION)
        return;
    if (valid)
        xEventGroupSetBits(ot_validation_event_group, OTA_VALID_FLAG);
    else
        xEventGroupSetBits(ot_validation_event_group, OTA_INVALID_FLAG);
}

void light_sleep(uint16_t seconds)
{
    Serial.printf("light sleeping for %d seconds\n", seconds);
    esp_sleep_enable_timer_wakeup(seconds * 1000000);
    esp_light_sleep_start();
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
}

void checkOTA_rollback(void *)
{
    auto event = xEventGroupWaitBits(ot_validation_event_group, OTA_VALID_FLAG, pdTRUE, pdFALSE, pdMS_TO_TICKS(30000));
    if (event & OTA_VALID_FLAG)
    {
        ESP_LOGW("OTA", "Diagnostics completed successfully! Continuing execution ...");
        esp_ota_mark_app_valid_cancel_rollback();
    }
    else
    {
        ESP_LOGE("OTA", "Diagnostics failed! Start rollback to the previous version ...");
        esp_ota_mark_app_invalid_rollback_and_reboot();
    }
}

bool check_rollback()
{
    ot_validation_event_group = xEventGroupCreate();
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK)
    {
        switch (ota_state)
        {
        case ESP_OTA_IMG_PENDING_VERIFY:
        {
            OTA_NEED_VALIDATION = true;
            xTaskCreate(checkOTA_rollback, "OTA_CHECK", 2048, NULL, 1, NULL);
            return true;
            break;
        }
        case ESP_OTA_IMG_ABORTED:
        {
            mqttLogger.println("ESP_OTA_IMG_ABORTED");
            break;
        }
        case ESP_OTA_IMG_VALID:
        {
            mqttLogger.println("ESP_OTA_IMG_VALID");
            break;
        }
        default:
            break;
        }
    }
    return false;
}