idf_component_register(SRCS "updater.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES app_update esp_hal_usb esp_timer hal driver ota dispatcher system usb_device)
