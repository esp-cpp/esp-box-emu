idf_component_register(
  INCLUDE_DIRS "include"
  SRC_DIRS "src"
  REQUIRES "driver" "esp_lcd" "spi_flash" "nvs_flash" "es8311" "esp-idf-cxx" "display" "display_drivers"
  )
