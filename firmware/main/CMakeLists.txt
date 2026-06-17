idf_component_register(
    SRCS "main.cpp"
    PRIV_REQUIRES app_bsp app_update bootloader_support esp_app_format esp_event esp_http_server esp_netif esp_partition esp_timer mbedtls port_bsp user_app
    INCLUDE_DIRS "./")
