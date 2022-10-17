idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS ".")
# 'littlefs' is the file system partition label 'flash_data' is the folder with
# data to flash, and FLASH_IN_PROJECT says to flash it when flashing project
# littlefs_create_partition_image(littlefs ../flash_data FLASH_IN_PROJECT)
littlefs_create_partition_image(littlefs ../flash_data)
