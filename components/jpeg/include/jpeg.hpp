#pragma once

#include <fstream>

#include "esp_heap_caps.h"

#include "format.hpp"
#include "jpegdec.h"

namespace Jpeg {
  static uint8_t *_decoded_data;
  static int _image_width;
  static int _image_height;
  static int _image_size;
  static int _offset;
  static std::ifstream _imgfile;
  static JPEGDEC decoder;

  void *open(const char *filename, int32_t *size) {
    if (_imgfile.is_open()) {
      _imgfile.close();
    }
    // open file at end
    _imgfile.open(filename, std::ios::binary | std::ios::ate);
    if (!_imgfile.is_open()) {
      fmt::print("Couldn't open {}\n", filename);
      size = 0;
      return nullptr;
    }
    // get size from current location (end)
    *size = (size_t)_imgfile.tellg();
    // reset file pointer to beginning
    _imgfile.seekg(0, std::ios::beg);
    return &_imgfile;
  }

  void close(void *handle) {
    if (_imgfile.is_open()) {
      // fmt::print("Closing file\n");
      _imgfile.close();
    }
  }

  int32_t read(JPEGFILE *handle, uint8_t *buffer, int32_t length) {
    if (!_imgfile.is_open()) {
      return 0;
    }
    // fmt::print("reading {} bytes\n", length);
    _imgfile.read((char*)buffer, length);
    return _imgfile.gcount();
  }

  int32_t seek(JPEGFILE *handle, int32_t position) {
    if (!_imgfile.is_open()) {
      return 0;
    }
    // fmt::print("seeking to {}\n", position);
    _imgfile.seekg(position, std::ios::beg);
    return 1;
  }

  int on_data_decode(JPEGDRAW *pDraw) {
    // NOTE: for some reason our images are 100px wide, but we keep getting
    // larger for iWidth. Therefore when we do our destination calculation we
    // need to use width, but when we do our source calculation we need to use
    // iWidth.
    int width = std::min(_image_width, pDraw->iWidth);
    int height = pDraw->iHeight;
    auto xs = pDraw->x;
    auto ys = pDraw->y;
    auto ye = pDraw->y + height - 1;
    uint16_t *dst_buffer = (uint16_t*)_decoded_data;
    uint16_t *src_buffer = (uint16_t*)pDraw->pPixels;
    // two bytes per pixel
    auto num_bytes_per_row = width * 2;
    for (int y=ys; y<=ye; y++) {
      int dst_offset = y * width + xs;
      int src_offset = (y-ys) * pDraw->iWidth + xs;
      memcpy(&dst_buffer[dst_offset], &src_buffer[src_offset], num_bytes_per_row);
    }
    return 1; // continue decode
  }

  int get_width() {
    return _image_width;
  }

  int get_height() {
    return _image_height;
  }

  uint8_t *get_decoded_data() {
    return _decoded_data;
  }

  int get_size() {
    return _image_size;
  }

  void decode(const char* filename) {
    // open the image
    _offset = 0;
    decoder.open(filename, open, close, read, seek, on_data_decode);
    decoder.setPixelType(RGB565_BIG_ENDIAN);
    // now that we've opened it we know what the decoded size will be, so
    // allocate a buffer to fill it with
    _image_width = decoder.getWidth();
    _image_height = decoder.getHeight();
    _image_size = _image_height * _image_width * 2;
    _decoded_data = (uint8_t*)heap_caps_malloc(_image_size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    // now actually decode it
    if (!decoder.decode(0, 0, 0)) {
      // couldn't decode it, free the memory we allocated
      free(_decoded_data);
      _decoded_data = nullptr;
      _image_size = 0;
      fmt::print("Couldn't decode!\n");
    }
    // close it
    decoder.close();
  }
}
