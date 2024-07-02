idf_component_register(
  INCLUDE_DIRS "include"
  SRC_DIRS "src"
  REQUIRES "driver" "heap" "fatfs" "esp_lcd" "esp_psram" "hal" "usb" "esp_tinyusb" "spi_flash" "nvs_flash" "codec" "adc" "aw9523" "button" "display" "display_drivers" "mcp23x17" "input_drivers" "tt21100" "gt911" "drv2605" "event_manager" "i2c" "task" "timer" "serialization" "max1704x" "esp-box"
  )
