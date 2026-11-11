idf_component_register(SRCS "updater.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES app_update esp_hal_usb hal driver ota dispatcher system usb_device)
