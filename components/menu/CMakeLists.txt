idf_component_register(
  INCLUDE_DIRS "include"
  SRC_DIRS "src" "generated" "generated/screens" "generated/components"
  PRIV_INCLUDE_DIRS "generated"
  REQUIRES lvgl task display logger jpeg box-emu-hal)
