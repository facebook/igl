/*
 * LightweightVK
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "LVK.h"

#include <assert.h>

namespace {

struct TextureFormatProperties {
  const lvk::TextureFormat format = lvk::TextureFormat::Invalid;
  const uint8_t bytesPerBlock : 5 = 1;
  const uint8_t blockWidth : 4 = 1;
  const uint8_t blockHeight : 4 = 1;
  const uint8_t minBlocksX : 2 = 1;
  const uint8_t minBlocksY : 2 = 1;
  const bool depth : 1 = false;
  const bool stencil : 1 = false;
  const bool compressed : 1 = false;
};

#define PROPS(fmt, bpb, ...) \
  TextureFormatProperties{ .format = lvk::TextureFormat::fmt, .bytesPerBlock = bpb, ##__VA_ARGS__ }

static constexpr TextureFormatProperties properties[] = {
    PROPS(Invalid, 1),
    PROPS(R_UN8, 1),
    PROPS(R_UI16, 2),
    PROPS(R_UN16, 2),
    PROPS(R_F16, 2),
    PROPS(R_F32, 4),
    PROPS(RG_UN8, 2),
    PROPS(RG_UI16, 4),
    PROPS(RG_UN16, 4),
    PROPS(RG_F16, 4),
    PROPS(RG_F32, 8),
    PROPS(RGBA_UN8, 4),
    PROPS(RGBA_UI32, 16),
    PROPS(RGBA_F16, 8),
    PROPS(RGBA_F32, 16),
    PROPS(RGBA_SRGB8, 4),
    PROPS(BGRA_UN8, 4),
    PROPS(BGRA_SRGB8, 4),
    PROPS(ETC2_RGB8, 8, .blockWidth = 4, .blockHeight = 4, .compressed = true),
    PROPS(ETC2_SRGB8, 8, .blockWidth = 4, .blockHeight = 4, .compressed = true),
    PROPS(BC7_RGBA, 16, .blockWidth = 4, .blockHeight = 4, .compressed = true),
    PROPS(Z_UN16, 2, .depth = true),
    PROPS(Z_UN24, 3, .depth = true),
    PROPS(Z_F32, 4, .depth = true),
    PROPS(Z_UN24_S_UI8, 4, .depth = true, .stencil = true),
};

} // namespace

static_assert(sizeof(TextureFormatProperties) <= sizeof(uint32_t));
static_assert(LVK_ARRAY_NUM_ELEMENTS(properties) == lvk::TextureFormat::Z_UN24_S_UI8 + 1);

bool lvk::isDepthOrStencilFormat(lvk::TextureFormat format) {
  return properties[format].depth || properties[format].stencil;
}

uint32_t lvk::getTextureBytesPerLayer(uint32_t width,
                                      uint32_t height,
                                      lvk::TextureFormat format,
                                      uint32_t level) {
  const uint32_t levelWidth = std::max(width >> level, 1u);
  const uint32_t levelHeight = std::max(height >> level, 1u);

  const auto props = properties[format];

  if (!props.compressed) {
    return props.bytesPerBlock * levelWidth * levelHeight;
  }

  const uint32_t blockWidth = std::max((uint32_t)props.blockWidth, 1u);
  const uint32_t blockHeight = std::max((uint32_t)props.blockHeight, 1u);
  const uint32_t widthInBlocks = (levelWidth + props.blockWidth - 1) / props.blockWidth;
  const uint32_t heightInBlocks = (levelHeight + props.blockHeight - 1) / props.blockHeight;
  return widthInBlocks * heightInBlocks * props.bytesPerBlock;
}

uint32_t lvk::calcNumMipLevels(uint32_t width, uint32_t height) {
  assert(width > 0);
  assert(height > 0);

  uint32_t levels = 1;

  while ((width | height) >> levels) {
    levels++;
  }

  return levels;
}

bool lvk::Assert(bool cond, const char* file, int line, const char* format, ...) {
  if (!cond) {
    va_list ap;
    va_start(ap, format);
    LLOGW("[LVK] Assertion failed in %s:%d: ", file, line);
    LLOGW(format, ap);
    va_end(ap);
    assert(false);
  }
  return cond;
}

void lvk::destroy(lvk::IDevice* device, lvk::ComputePipelineHandle handle) {
  if (device) {
    device->destroy(handle);
  }
}

void lvk::destroy(lvk::IDevice* device, lvk::RenderPipelineHandle handle) {
  if (device) {
    device->destroy(handle);
  }
}

void lvk::destroy(lvk::IDevice* device, lvk::ShaderModuleHandle handle) {
  if (device) {
    device->destroy(handle);
  }
}

// Logs GLSL shaders with line numbers annotation
void lvk::logShaderSource(const char* text) {
  uint32_t line = 1;

  LLOGL("\n(%3u) ", line);

  while (text && *text) {
    if (*text == '\n') {
      LLOGL("\n(%3u) ", ++line);
    } else if (*text == '\r') {
      // skip it to support Windows/UNIX EOLs
    } else {
      LLOGL("%c", *text);
    }
    text++;
  }
  LLOGL("\n");
}
