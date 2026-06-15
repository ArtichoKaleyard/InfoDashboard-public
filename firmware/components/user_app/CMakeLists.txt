idf_component_register(
    SRCS
        "user_app.cpp"
        "dashboard_style.cpp"
        "dashboard_widgets.cpp"
        "dashboard_font_ui_10_1bpp.c"
        "dashboard_font_ui_12_1bpp.c"
        "dashboard_font_ui_consolas_regular_14_1bpp.c"
        "dashboard_font_ui_consolas_bold_14_1bpp.c"
        "dashboard_font_ui_16_1bpp.c"
        "dashboard_font_ui_16m_1bpp.c"
        "dashboard_font_ui_segoe_semibold_16_1bpp.c"
        "dashboard_font_ui_28_1bpp.c"
    PRIV_REQUIRES app_bsp esp-tls esp_adc esp_driver_gpio esp_driver_i2c esp_event esp_http_client esp_netif esp_timer esp_wifi json lvgl__lvgl lwip nvs_flash
    INCLUDE_DIRS "./")
