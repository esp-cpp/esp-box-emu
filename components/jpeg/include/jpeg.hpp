#pragma once

#include <fstream>

#include "esp_heap_caps.h"
#include "lvgl.h"

#include "spi_lcd.h"

#include "format.hpp"
#include "jpegdec.h"

class Jpeg {
public:
  static constexpr size_t max_encoded_size = 100 * 1024;
  static constexpr size_t max_decoded_size = 100*200*2;

  Jpeg() {
    if (!encoded_data_) encoded_data_ = get_frame_buffer1();
    if (!decoded_data_) decoded_data_ = (uint8_t*)heap_caps_malloc(max_decoded_size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
  }

  ~Jpeg() {
    if (encoded_data_) {
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
    decoder_.setPixelType(RGB565_BIG_ENDIAN);
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
      size = 0;
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
    // NOTE: for some reason our images are 100px wide, but we keep getting
    // larger for iWidth. Therefore when we do our destination calculation we
    // need to use width, but when we do our source calculation we need to use
    // iWidth.
    int width = std::min(image_width_, pDraw->iWidth);
    int height = pDraw->iHeight;
    auto xs = pDraw->x;
    auto ys = pDraw->y;
    auto ye = pDraw->y + height - 1;
    uint16_t *dst_buffer = (uint16_t*)decoded_data_;
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

  static uint8_t *encoded_data_;
  static uint8_t *decoded_data_;
  static int image_width_;
  static int image_height_;
  static int image_size_;
  std::ifstream imgfile_;
  JPEGDEC decoder_;
};
