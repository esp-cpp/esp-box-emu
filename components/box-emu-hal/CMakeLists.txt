idf_component_register(
  INCLUDE_DIRS "include"
  SRC_DIRS "src"
  REQUIRES "driver" "heap" "esp_lcd" "esp_psram" "spi_flash" "nvs_flash" "codec" "display" "display_drivers" "controller" "ads1x15" "qwiicnes" "input_drivers" "ft5x06" "tt21100" "drv2605"
  )
