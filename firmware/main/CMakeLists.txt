idf_component_register(
    SRCS "main.cpp"
    PRIV_REQUIRES app_bsp esp_event esp_http_server esp_netif esp_timer port_bsp user_app
    INCLUDE_DIRS "./")
