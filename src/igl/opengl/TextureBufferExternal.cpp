/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/TextureBufferExternal.h>

namespace igl::opengl {
TextureBufferExternal::TextureBufferExternal(IContext& context,
                                             TextureFormat format,
                                             TextureDesc::TextureUsage usage) :
  Super(context, format) {
  FormatDescGL formatDescGL;
  toFormatDescGL(format, usage, formatDescGL);
  glInternalFormat_ = formatDescGL.internalFormat;

  setUsage(usage);
}

} // namespace igl::opengl
