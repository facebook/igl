/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <igl/opengl/webgl/Context.h>
#include <igl/opengl/webgl/Device.h>
#include <igl/opengl/webgl/HWDevice.h>

namespace igl::opengl::webgl {

std::unique_ptr<IContext> HWDevice::createContext(Result* outResult) const {
  Result::setOk(outResult);
  return std::make_unique<Context>();
}

std::unique_ptr<IContext> HWDevice::createContext(RenderingAPI api,
                                                  EGLNativeWindowType nativeWindow,
                                                  Result* outResult) const {
  return api == RenderingAPI::GLES2
             ? createContext(
                   {.flavor = BackendFlavor::OpenGL_ES, .majorVersion = 2, .minorVersion = 0},
                   nativeWindow,
                   outResult)
             : createContext(
                   {.flavor = BackendFlavor::OpenGL_ES, .majorVersion = 3, .minorVersion = 0},
                   nativeWindow,
                   outResult);
}

std::unique_ptr<IContext> HWDevice::createContext(BackendVersion backendVersion,
                                                  EGLNativeWindowType /*nativeWindow*/,
                                                  Result* outResult) const {
  Result::setOk(outResult);
  return std::make_unique<Context>(backendVersion);
}

std::unique_ptr<opengl::Device> HWDevice::createWithContext(std::unique_ptr<IContext> context,
                                                            Result* outResult) const {
  Result::setOk(outResult);
  return std::make_unique<opengl::webgl::Device>(std::move(context));
}

} // namespace igl::opengl::webgl
