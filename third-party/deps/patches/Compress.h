// Compress.h - A BC7 wrapper class
// test.cpp file is referenced and some code blocks
// are copied to create this file

// Based on https://github.com/richgel999/bc7enc/blob/master/test.cpp

#pragma once

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <atomic>
#include <vector>

#include "bc7enc.h"

const int MAX_UBER_LEVEL = 5;

struct block16 {
  uint64_t values_[2];
};

typedef std::vector<block16> block16_vec;

class Compress {
 private:
  struct color_quad_u8 {
    uint8_t colors_[4];

    inline uint8_t& operator[](uint32_t i) {
      assert(i < 4);
      return colors_[i];
    }
    inline uint8_t operator[](uint32_t i) const {
      assert(i < 4);
      return colors_[i];
    }
  };
  typedef std::vector<color_quad_u8> color_quad_u8_vec;

  class image_u8 {
   public:
    image_u8(uint32_t width, uint32_t height, uint32_t channel) :
      width_(width), height_(height), channel_(channel) {
      pixels_.resize((uint32_t)width * height);
    }

    image_u8(uint8_t* p, uint32_t width, uint32_t height, uint32_t channel) :
      width_(width), height_(height), channel_(channel) {
      pixels_.resize((uint32_t)(width * height));
      memcpy(&pixels_[0], p, (uint32_t)(width * height * channel_));
    }

    inline Compress::color_quad_u8_vec& get_pixels() {
      return pixels_;
    }

    inline const Compress::color_quad_u8_vec& get_pixels() const {
      return pixels_;
    }

    inline uint32_t width() const {
      return width_;
    }
    inline uint32_t height() const {
      return height_;
    }
    inline uint32_t total_pixels() const {
      return width_ * height_;
    }

    inline Compress::color_quad_u8& operator()(uint32_t x, uint32_t y) {
      assert(x < width_ && y < height_);
      return pixels_[(uint32_t)(x + width_ * y)];
    }
    inline const Compress::color_quad_u8& operator()(uint32_t x, uint32_t y) const {
      assert(x < width_ && y < height_);
      return pixels_[(uint32_t)(x + width_ * y)];
    }

    image_u8& crop(uint32_t new_width, uint32_t new_height, uint32_t new_channel);

    image_u8& swap(image_u8& other);

    void get_block(uint32_t bx,
                   uint32_t by,
                   uint32_t width,
                   uint32_t height,
                   Compress::color_quad_u8* pPixels);

   private:
    color_quad_u8_vec pixels_;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    uint32_t channel_ = 0;
  };

 public:
  static block16_vec getCompressedImage(
      uint8_t* pixels,
      uint32_t w,
      uint32_t h,
      uint32_t c,
      bool flipVertical = false,
      std::atomic<bool>* shouldStop = nullptr);
};
