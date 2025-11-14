#include "main.hpp"

#include "esp_ota_ops.h"
#include "esp_log.h"

void checkOTA_rollback(void *arg)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK)
    {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY)
        {
            OTA_VALIDATION = true;
            // run diagnostic function ...
            delay(5000);
            bool diagnostic_is_ok = (OTA_VALIDATION_COUNTER > 100U);
            if (diagnostic_is_ok)
            {
                ESP_LOGW("OTA", "Diagnostics completed successfully! Continuing execution ...");
                esp_ota_mark_app_valid_cancel_rollback();
            }
            else
            {
                ESP_LOGE("OTA", "Diagnostics failed! Start rollback to the previous version ...");
                esp_ota_mark_app_invalid_rollback_and_reboot();
            }
            OTA_VALIDATION = false;
        }
    }
    vTaskDelete(NULL);
}