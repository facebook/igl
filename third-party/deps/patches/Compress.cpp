// Compress.cpp - A BC7 wrapper class
// test.cpp file is referenced and some code blocks
// are copied to create this file

// Based on https://github.com/richgel999/bc7enc/blob/master/test.cpp

#include "Compress.h"

#include <algorithm>

Compress::image_u8& Compress::image_u8::crop(uint32_t new_width,
                                             uint32_t new_height,
                                             uint32_t new_channel) {
  if ((width_ == new_width) && (height_ == new_height)) {
    return *this;
  }

  image_u8 new_image(new_width, new_height, new_channel);

  const uint32_t w = std::min(width_, new_width);
  const uint32_t h = std::min(height_, new_height);

  for (uint32_t y = 0; y < h; y++) {
    for (uint32_t x = 0; x < w; x++) {
      new_image(x, y) = (*this)(x, y);
    }
  }

  return swap(new_image);
}

Compress::image_u8& Compress::image_u8::swap(Compress::image_u8& other) {
  std::swap(width_, other.width_);
  std::swap(height_, other.height_);
  std::swap(pixels_, other.pixels_);

  return *this;
}

void Compress::image_u8::get_block(uint32_t bx,
                                   uint32_t by,
                                   uint32_t width,
                                   uint32_t height,
                                   Compress::color_quad_u8* pPixels) {
  assert((bx * width + width) <= width_);
  assert((by * height + height) <= height_);

  for (uint32_t y = 0; y < height; y++) {
    memcpy(
        pPixels + y * width, &(*this)(bx * width, by * height + y), width * sizeof(color_quad_u8));
  }
}

block16_vec Compress::getCompressedImage(uint8_t* pixels,
                                         uint32_t w,
                                         uint32_t h,
                                         uint32_t c,
    bool flipVertical,
    std::atomic<bool>* shouldStop) {
  const bool perceptual = true;
  const uint32_t pixel_format_bpp = 8;

  const uint32_t bytes_per_block = (16 * pixel_format_bpp) / 8;
  assert(bytes_per_block == 8 || bytes_per_block == 16);

  image_u8 source_image(pixels, w, h, c);

   // Flip image across y-axis
  if (flipVertical) {
    image_u8 yFlippedImage(pixels, w, h, c);

    for (uint32_t y = 0; y < h; y++) {
      for (uint32_t x = 0; x < w; x++) {
        yFlippedImage(x, (h - 1) - y) = source_image(x, y);
      }
    }
    yFlippedImage.swap(source_image);
  }

  // crop input image
  source_image.crop((source_image.width() + 3) & ~3, (source_image.height() + 3) & ~3, c);

  const uint32_t blocks_x = source_image.width() / 4;
  const uint32_t blocks_y = source_image.height() / 4;

  // output compressed image
  block16_vec packed_image16((int32_t)(blocks_x * blocks_y));

  // initialize compression parameters
  bc7enc_compress_block_params pack_params;
  bc7enc_compress_block_params_init(&pack_params);
  if (!perceptual) {
    bc7enc_compress_block_params_init_linear_weights(&pack_params);
  }

  pack_params.m_max_partitions_mode = BC7ENC_MAX_PARTITIONS1;
  pack_params.m_uber_level = 0;
  bc7enc_compress_block_init();

  bool has_alpha = false;

  // compression is done per block-by-block basis
  for (uint32_t by = 0; by < blocks_y; by++) {
    for (uint32_t bx = 0; bx < blocks_x; bx++) {
      if (shouldStop && shouldStop->load(std::memory_order_acquire)) {
        // compression is very slow, so there is an early exit if we are
        // required to stop
        return packed_image16;
      }

      color_quad_u8 pixels[16];

      source_image.get_block(bx, by, 4, 4, pixels);
      if (!has_alpha) {
        for (uint32_t i = 0; i < 16; i++) {
          if (pixels[i].colors_[3] < 255) {
            has_alpha = true;
          }
        }
      }

      block16* pBlock = &packed_image16[(uint32_t)(bx + by * blocks_x)];
      bc7enc_compress_block(pBlock, pixels, &pack_params);
    }
  }

  return packed_image16;
}
