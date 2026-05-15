idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "includes"
    REQUIRES nvs_flash esp_wifi esp_netif esp_event esp_http_server esp_timer esp_driver_usb_serial_jtag vfs
)
