idf_component_register(
  INCLUDE_DIRS "include"
  SRC_DIRS "src"
  PRIV_REQUIRES esp_system
  REQUIRES app_update coredump dispatcher monitor system box-emu)
# no OTA service in the emulator: an update goes through the updater app in
# ota_1 (see updater/). Writing the flash from the emulator, which runs its
# code from PSRAM (SPIRAM_FETCH_INSTRUCTIONS), corrupts its PSRAM heap and
# fails the image check.
target_compile_definitions(${COMPONENT_LIB} PUBLIC DEVLINK_HAS_OTA=0)
# keep the last panic's reason across the reset (DevLink::last_panic())
target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_panic_handler" "-Wl,--wrap=panic_abort")
