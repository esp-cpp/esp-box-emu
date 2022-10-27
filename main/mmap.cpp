#include "mmap.hpp"

const esp_partition_t* nesgame_partition;
spi_flash_mmap_handle_t hrom;

void init_memory() {
  // Initialize NVS, needed for BT
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    fmt::print("Erasing NVS flash...\n");
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  // get the romdata (nesgame) partition, type 0x40, subtype 1 (partitions.csv)
  nesgame_partition = esp_partition_find_first((esp_partition_type_t)0x40, (esp_partition_subtype_t)1, NULL);
  if (!nesgame_partition) {
    fmt::print(fg(fmt::color::red), "Couldn't find nesgame_partition!\n");
  }
}

size_t copy_romdata_to_nesgame_partition(const std::string& rom_filename) {
  esp_err_t err;
  // erase the existing rom (if any) in the nesgame_partition
  err = esp_partition_erase_range(nesgame_partition, 0, nesgame_partition->size);
  if (err != ESP_OK) {
    fmt::print("Couldn't erase nesgame_partition!\n");
  }
  // load the file data and iteratively copy it over
  std::ifstream romfile(rom_filename, std::ios::binary | std::ios::ate); //open file at end
  if (!romfile.is_open()) {
    fmt::print("Error: ROM file does not exist\n");
    return 0;
  }

  size_t filesize = romfile.tellg(); // get size from current file pointer location;
  romfile.seekg(0, std::ios::beg); //reset file pointer to beginning;

  size_t block_size = 1024*4; // 4K
  size_t bytes_written = 0;
  char block[block_size];
  for (size_t offset=0; offset < filesize; offset += block_size) {
    size_t read_size = std::min(filesize - offset, block_size);
    romfile.read(block, read_size);
    err = esp_partition_write(nesgame_partition, offset, block, read_size);
    if (err != ESP_OK) {
      fmt::print("Couldn't write to nesgame_partition, offset: {}, read_size: {}!\n", offset, read_size);
    }
    bytes_written += read_size;
  }
  fmt::print("Copied {} bytes to nesgame_partition\n", bytes_written);
  romfile.close();
  return bytes_written;
}

uint8_t *get_mmapped_romdata() {
  uint8_t* romdata;
  esp_err_t err;
  err = esp_partition_mmap(nesgame_partition, 0, 3*1024*1024, SPI_FLASH_MMAP_DATA, (const void**)&romdata, &hrom);
  if (err != ESP_OK) {
    fmt::print("Couldn't map nesgame_partition!\n");
    return nullptr;
  }
  fmt::print("Initialized. ROM@{}\n", fmt::ptr(romdata));
  return romdata;
}
