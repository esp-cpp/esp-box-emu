idf_component_register(
  INCLUDE_DIRS "include"
  SRC_DIRS "src" "generated"
  PRIV_INCLUDE_DIRS "generated"
  REQUIRES lvgl task display logger jpeg box-emu-hal)
