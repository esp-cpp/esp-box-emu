idf_component_register(
  INCLUDE_DIRS "include"
  SRC_DIRS "src"
  REQUIRES "driver" "heap" "fatfs" "esp_lcd" "esp_psram" "spi_flash" "nvs_flash" "codec" "display" "display_drivers" "mcp23x17" "input_drivers" "tt21100" "gt911" "drv2605" "event_manager" "i2c"
  )
