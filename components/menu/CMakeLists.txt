idf_component_register(
  INCLUDE_DIRS "include"
  PRIV_INCLUDE_DIRS "generated"
  SRC_DIRS "src" "generated" "generated/screens" "generated/components" "generated/images"
  REQUIRES lvgl timer display logger jpeg box-emu statistics)
