/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <EGL/egl.h>
#include <igl/Texture.h>
#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/PlatformDevice.h>

#if defined(IGL_ANDROID_HWBUFFER_SUPPORTED)
struct AHardwareBuffer;
#endif // defined(IGL_ANDROID_HWBUFFER_SUPPORTED)

namespace igl::opengl {

class ViewTextureTarget;

namespace egl {

class Device;
class Context;

class PlatformDevice : public opengl::PlatformDevice {
 public:
  static constexpr igl::PlatformDeviceType Type = igl::PlatformDeviceType::OpenGLEgl;

  explicit PlatformDevice(Device& owner);
  ~PlatformDevice() override = default;

  /// Returns a texture representing the EGL Surface associated with this device's context.
  std::shared_ptr<ITexture> createTextureFromNativeDrawable(Result* outResult);

  std::shared_ptr<ITexture> createTextureFromNativeDrawable(int width,
                                                            int height,
                                                            Result* outResult);

  /// Returns a texture representing the EGL depth texture associated with this device's context.
  std::shared_ptr<ITexture> createTextureFromNativeDepth(TextureFormat depthTextureFormat,
                                                         Result* outResult);

#if defined(IGL_ANDROID_HWBUFFER_SUPPORTED)
  /// returns a android::NativeHWTextureBuffer on platforms supporting it
  /// this texture allows CPU and GPU to both read/write memory
  std::shared_ptr<ITexture> createTextureWithSharedMemory(const TextureDesc& desc,
                                                          Result* outResult) const;
  std::shared_ptr<ITexture> createTextureWithSharedMemory(AHardwareBuffer* buffer,
                                                          Result* outResult) const;
#endif // defined(IGL_ANDROID_HWBUFFER_SUPPORTED)

  /// This function must be called every time the currently bound EGL read and/or draw surfaces
  /// change, in order to notify IGL of these changes.
  void updateSurfaces(EGLSurface readSurface, EGLSurface drawSurface, Result* outResult);

  EGLSurface createSurface(NativeWindowType nativeWindow, Result* outResult);

  EGLSurface getReadSurface(Result* outResult);

  void setPresentationTime(long long presentationTimeNs, Result* outResult);

 protected:
  [[nodiscard]] bool isType(PlatformDeviceType t) const noexcept override;

 private:
  std::shared_ptr<ViewTextureTarget> drawableTexture_;

  std::pair<EGLint, EGLint> getSurfaceDimensions(const Context& context, Result* outResult);
};

} // namespace egl
} // namespace igl::opengl
