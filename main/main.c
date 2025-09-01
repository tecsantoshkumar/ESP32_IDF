#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/param.h>
#include <time.h>
#include <sys/time.h>

// ESP-IDF core
#include "esp_log.h"
#include "esp_check.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_tls.h"
#include "esp_tls_crypto.h"

// FreeRTOS / Drivers
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"

// NimBLE
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gatt.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

// HTTP + MQTT
#include "esp_http_server.h"
#include "mqtt_client.h"
#include "MQTT_Handeller.h"
#include "HTTP_Handeller.h"
#include "BLE_Handeller.h"

// Wi-Fi Provisioning
#include "wifi_provisioning/manager.h"
#include <wifi_provisioning/scheme_ble.h>
#include <wifi_provisioning/scheme_softap.h>
#include "qrcode.h"
#include "protocol_examples_common.h"
#include "protocol_examples_utils.h"

#if !CONFIG_IDF_TARGET_LINUX
    #include <esp_wifi.h>
    #include <esp_system.h>
    #include "esp_eth.h"
#endif

static const char *TAG = "app";
static bool is_provisioned = false;
static httpd_handle_t server = NULL;

/* ------------------ Helpers ------------------ */
static void get_device_service_name(char *service_name, size_t max)
{
    uint8_t eth_mac[6];
    const char *ssid_prefix = "PROV_";
    esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
    snprintf(service_name, max, "%s%02X%02X%02X",
             ssid_prefix, eth_mac[3], eth_mac[4], eth_mac[5]);
}

static void wifi_prov_print_qr(const char *name, const char *pop, const char *transport)
{
    char payload[150] = {0};
    if (pop) {
        snprintf(payload, sizeof(payload),
                 "{\"ver\":\"v1\",\"name\":\"%s\",\"pop\":\"%s\",\"transport\":\"%s\"}",
                 name, pop, transport);
    } else {
        snprintf(payload, sizeof(payload),
                 "{\"ver\":\"v1\",\"name\":\"%s\",\"transport\":\"%s\"}",
                 name, transport);
    }
    ESP_LOGI("PROV", "Scan this QR in ESP Provisioning app:\n");
    esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
    esp_qrcode_generate(&cfg, payload);
}

/* ------------------ Start Services ------------------ */
static void start_services(void)
{
    ESP_LOGI(TAG, "Starting BLE GATT + HTTP + MQTT services...");

    // BLE GATT services
    // ble_svc_gap_device_name_set("BLE-Server");
    // ble_svc_gap_init();
    // ble_svc_gatt_init();
    // ble_gatts_count_cfg(gatt_svcs);
    // ble_gatts_add_svcs(gatt_svcs);
    // ble_hs_cfg.sync_cb = ble_app_on_sync;

    // HTTP + MQTT
    server = start_webserver();
    mqtt_app_start();
}

/* ------------------ Provisioning ------------------ */
static void provisioning_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_prov_mgr_config_t prov_cfg = {
        .scheme = wifi_prov_scheme_ble,
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM
    };
    ESP_ERROR_CHECK(wifi_prov_mgr_init(prov_cfg));

    bool provisioned = false;
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));

    if (!provisioned) {
        ESP_LOGI("PROV", "Starting provisioning");

        char service_name[12];
        get_device_service_name(service_name, sizeof(service_name));
        const char *pop = "abcd1234";  // proof-of-possession

        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(
            WIFI_PROV_SECURITY_1, pop, service_name, NULL));

        wifi_prov_print_qr(service_name, pop, "ble");
    } else {
        ESP_LOGI("PROV", "Already provisioned, starting Wi-Fi STA");
        wifi_prov_mgr_deinit();
        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_start();

        // Directly start services since already provisioned
        start_services();
    }
}

/* ------------------ Main ------------------ */
void app_main(void)
{
    // NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_ERROR_CHECK(gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT));

    // Init provisioning / auto-start services if already provisioned
    provisioning_init();
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
