#include "mmap.hpp"
#include <fstream>
#include "esp_heap_caps.h"

static uint8_t* romdata = nullptr;

void init_memory() {
  // allocate memory for the ROM and make sure it's on the SPIRAM
  romdata = (uint8_t*)heap_caps_malloc(4*1024*1024, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
  if (romdata == nullptr) {
    fmt::print(fg(fmt::terminal_color::red), "ERROR: Couldn't allocate memory for ROM!\n");
  }
}

size_t copy_romdata_to_cart_partition(const std::string& rom_filename) {
  // load the file data and iteratively copy it over
  std::ifstream romfile(rom_filename, std::ios::binary | std::ios::ate); //open file at end
  if (!romfile.is_open()) {
    fmt::print("Error: ROM file does not exist\n");
    return 0;
  }
  size_t filesize = romfile.tellg(); // get size from current file pointer location;
  romfile.seekg(0, std::ios::beg); //reset file pointer to beginning;
  romfile.read((char*)(romdata), filesize);
  romfile.close();
  return filesize;
}

extern "C" uint8_t *osd_getromdata() {
  return get_mmapped_romdata();
}

uint8_t *get_mmapped_romdata() {
  return romdata;
}
