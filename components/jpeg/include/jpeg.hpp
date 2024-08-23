#pragma once

#include <fstream>

#include "esp_heap_caps.h"
#include "lvgl.h"

#include "format.hpp"
#include "JPEGDEC.h"

class Jpeg {
public:
  static constexpr size_t max_encoded_size = 100 * 1024;
  static constexpr size_t max_decoded_size = 100*200*2;

  Jpeg() {
    if (!encoded_data_) encoded_data_ = (uint8_t*)heap_caps_malloc(max_encoded_size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    if (!decoded_data_) decoded_data_ = (uint8_t*)heap_caps_malloc(max_decoded_size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
  }

  ~Jpeg() {
    if (encoded_data_) {
      heap_caps_free(encoded_data_);
      encoded_data_ = nullptr;
    }
    if (decoded_data_) {
      heap_caps_free(decoded_data_);
      decoded_data_ = nullptr;
    }
  }

  void decode(const char* filename) {
    // open the image
    int32_t encoded_length = 0;
    open(filename, &encoded_length);
    read(encoded_data_, encoded_length);
    close();
    decoder_.openRAM(encoded_data_, encoded_length, &Jpeg::on_data_decode);
    decoder_.setPixelType(RGB565_LITTLE_ENDIAN);
    image_width_ = decoder_.getWidth();
    image_height_ = decoder_.getHeight();
    image_size_ = image_height_ * image_width_ * 2;
    // now actually decode it
    if (!decoder_.decode(0, 0, 0)) {
      image_size_ = 0;
      image_width_ = 0;
      image_height_ = 0;
      fmt::print("Couldn't decode!\n");
    }
    decoder_.close();
  }

  int get_width() {
    return image_width_;
  }

  int get_height() {
    return image_height_;
  }

  uint8_t *get_decoded_data() {
    return decoded_data_;
  }

  int get_size() {
    return image_size_;
  }

protected:
  void open(const char *filename, int32_t *size) {
    if (imgfile_.is_open()) {
      imgfile_.close();
    }
    // open file at end
    imgfile_.open(filename, std::ios::binary | std::ios::ate);
    if (!imgfile_.is_open()) {
      fmt::print("Couldn't open {}\n", filename);
      *size = 0;
      return;
    }
    // get size from current location (end)
    *size = (size_t)imgfile_.tellg();
    // reset file pointer to beginning
    imgfile_.seekg(0, std::ios::beg);
  }

  void close() {
    if (imgfile_.is_open()) {
      imgfile_.close();
    }
  }

  int32_t read(uint8_t *buffer, int32_t length) {
    if (!imgfile_.is_open()) {
      return 0;
    }
    imgfile_.read((char*)buffer, length);
    return imgfile_.gcount();
  }

  static int on_data_decode(JPEGDRAW *pDraw) {
    auto width = pDraw->iWidth;
    auto height = pDraw->iHeight;
    auto xs = pDraw->x;
    auto ys = pDraw->y;
    uint16_t *dst_buffer = (uint16_t*)decoded_data_;
    const uint16_t *src_buffer = (const uint16_t*)pDraw->pPixels;
    // two bytes per pixel for RGB565
    uint16_t num_bytes_per_row = width * 2;
    for (uint16_t i = 0; i < height; i++) {
      uint16_t y = ys + i;
      uint16_t dst_offset = y * image_width_ + xs;
      uint16_t src_offset = i * width + xs;
      memcpy(&dst_buffer[dst_offset], &src_buffer[src_offset], num_bytes_per_row);
    }
    // continue decode
    return 1;
  }

  static uint8_t *encoded_data_;
  static uint8_t *decoded_data_;
  static int image_width_;
  static int image_height_;
  static int image_size_;
  std::ifstream imgfile_;
  JPEGDEC decoder_;
};
