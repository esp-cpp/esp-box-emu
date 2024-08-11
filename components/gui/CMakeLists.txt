idf_component_register(
  INCLUDE_DIRS "include"
  PRIV_INCLUDE_DIRS "generated"
  SRC_DIRS "src" "generated" "generated/screens" "generated/components" "generated/images"
  REQUIRES lvgl task display logger jpeg rom_info box-emu)
