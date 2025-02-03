idf_component_register(SRCS "hid_host_example.c" 
                            "ble_hid_keyboard.c"
                            "esp_hidd_prf_api.c"
                            "hid_dev.c"
                            "hid_device_le_prf.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES usb esp_driver_gpio bt nvs_flash esp_hid
                       )
set(CONFIG_MINIMAL_BUILD ON)
add_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=all -Wno-error=unused-variable -Wno-error=unused-const-variable -Wno-error=unused-function)
