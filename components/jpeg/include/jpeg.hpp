#pragma once

#include <fstream>

#include "esp_heap_caps.h"
#include "lvgl.h"

#include "spi_lcd.h"

#include "format.hpp"
#include "jpegdec.h"

namespace Jpeg {
  static constexpr size_t max_encoded_size = 100 * 1024;
  static constexpr size_t max_decoded_size = 100*200*2;
  static uint8_t *_encoded_data;
  static uint8_t *_decoded_data;
  static int _image_width;
  static int _image_height;
  static int _image_size;
  static std::ifstream _imgfile;
  static bool _initialized = false;

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
      _imgfile.close();
    }
  }

  int32_t read(JPEGFILE *handle, uint8_t *buffer, int32_t length) {
    if (!_imgfile.is_open()) {
      return 0;
    }
    _imgfile.read((char*)buffer, length);
    return _imgfile.gcount();
  }

  int32_t seek(JPEGFILE *handle, int32_t position) {
    if (!_imgfile.is_open()) {
      return 0;
    }
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
    // two bytes per pixel for RGB565
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

  void decode(JPEGDEC* _decoder, const char* filename) {
    // open the image
    int32_t encoded_length;
    open(filename, &encoded_length);
    read(nullptr, _encoded_data, encoded_length);
    close(nullptr);
    _decoder->openRAM(_encoded_data, encoded_length, on_data_decode);
    _decoder->setPixelType(RGB565_BIG_ENDIAN);
    _image_width = _decoder->getWidth();
    _image_height = _decoder->getHeight();
    _image_size = _image_height * _image_width * 2;
    // now actually decode it
    if (!_decoder->decode(0, 0, 0)) {
      _image_size = 0;
      _image_width = 0;
      _image_height = 0;
      fmt::print("Couldn't decode!\n");
    }
    _decoder->close();
  }

  void init() {
    if (_initialized) return;
    _encoded_data = get_frame_buffer1();
    _decoded_data = (uint8_t*)heap_caps_malloc(max_decoded_size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    _initialized = true;
  }
}
