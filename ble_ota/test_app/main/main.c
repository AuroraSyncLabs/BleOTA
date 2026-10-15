#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "ble_ota.h"

static const char *TAG = "ble_ota_test";

/* Required by the component: guards indication sends (declared extern in src/). */
SemaphoreHandle_t notify_sem;

static void recv_fw_cb(uint8_t *buf, uint32_t length)
{
    ESP_LOGI(TAG, "Received %" PRIu32 " bytes", length);
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    notify_sem = xSemaphoreCreateCounting(1, 1);

#if !CONFIG_BLE_OTA_EXTERNAL_HOST
    ESP_ERROR_CHECK(esp_ble_ota_host_init());
#endif
    ESP_ERROR_CHECK(esp_ble_ota_recv_fw_data_callback(recv_fw_cb));
}
