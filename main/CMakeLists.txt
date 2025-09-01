set(requires
    esp-tls
    nvs_flash
    esp_netif
    esp_http_server
    esp_driver_gpio
    esp_driver_uart
    wifi_provisioning
    esp_wifi
    esp_eth
    bt
    mqtt
)

idf_build_get_property(target IDF_TARGET)

if(${target} STREQUAL "linux")
    list(APPEND requires esp_stubs protocol_examples_common)
else()
    list(APPEND requires esp_wifi esp_eth)
endif()

idf_component_register(
    SRCS 
        "main.c"
        "BLE_Handeller.c"
        "MQTT_Handeller.c"
        "HTTP_Handeller.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES ${requires}
)
