#pragma once

#include <algorithm>
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
    image_size_ = 0;
    image_width_ = 0;
    image_height_ = 0;
    if (!encoded_data_ || !decoded_data_) {
      return;
    }
    // open the image
    int32_t encoded_length = 0;
    open(filename, &encoded_length);
    if (encoded_length <= 0 || encoded_length > (int32_t)max_encoded_size) {
      if (encoded_length > 0)
        fmt::print("{}: {} bytes, more than the {} the decoder takes\n", filename, encoded_length, max_encoded_size);
      close();
      return;
    }
    encoded_length = read(encoded_data_, encoded_length);
    close();
    if (!decoder_.openRAM(encoded_data_, encoded_length, &Jpeg::on_data_decode)) {
      fmt::print("{}: not a JPEG the decoder can read\n", filename);
      return;
    }
    decoder_.setPixelType(RGB565_LITTLE_ENDIAN);
    const int width = decoder_.getWidth();
    const int height = decoder_.getHeight();
    if ((size_t)width * height * 2 > max_decoded_size) {
      fmt::print("{}: {}x{} is larger than the decoder's {} byte buffer\n", filename, width, height, max_decoded_size);
      decoder_.close();
      return;
    }
    image_width_ = width;
    image_height_ = height;
    // now actually decode it
    if (decoder_.decode(0, 0, 0)) {
      image_size_ = width * height * 2;
    } else {
      image_width_ = 0;
      image_height_ = 0;
      fmt::print("{}: couldn't decode ({})\n", filename, decoder_.getLastError());
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
    // pPixels holds just this block: iWidth x iHeight, placed at (x, y)
    const int width = pDraw->iWidth;
    const int xs = pDraw->x;
    const int ys = pDraw->y;
    if (xs < 0 || ys < 0 || xs >= image_width_) {
      return 1;
    }
    const int copy_width = std::min(width, image_width_ - xs);
    const int height = std::min(pDraw->iHeight, image_height_ - ys);
    uint16_t *dst_buffer = reinterpret_cast<uint16_t*>(decoded_data_);
    const uint16_t *src_buffer = (const uint16_t*)pDraw->pPixels;
    for (int i = 0; i < height; i++) {
      memcpy(&dst_buffer[(size_t)(ys + i) * image_width_ + xs], &src_buffer[(size_t)i * width],
             copy_width * 2);
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
