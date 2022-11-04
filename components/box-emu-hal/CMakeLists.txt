idf_component_register(
  INCLUDE_DIRS "include"
  SRC_DIRS "src"
  REQUIRES "driver" "esp_lcd" "spi_flash" "nvs_flash" "codec" "display" "display_drivers" "controller" "ads1x15" "qwiicnes"
  )
