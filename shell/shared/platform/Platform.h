/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Device.h>
#include <memory>

namespace igl::shell {

class Extension;
class FileLoader;
class ImageLoader;
struct ImageData;
class ImageWriter;
class InputDispatcher;
class DisplayContext;

class Platform {
 public:
  Platform() noexcept;
  virtual ~Platform();
  virtual IDevice& getDevice() noexcept = 0;
  [[nodiscard]] virtual std::shared_ptr<IDevice> getDevicePtr() const noexcept = 0;
  virtual ImageLoader& getImageLoader() noexcept = 0;
  [[nodiscard]] virtual const ImageWriter& getImageWriter() const noexcept = 0;
  [[nodiscard]] virtual FileLoader& getFileLoader() const noexcept = 0;

  virtual InputDispatcher& getInputDispatcher() noexcept;
  [[nodiscard]] virtual DisplayContext& getDisplayContext() noexcept;

  std::shared_ptr<ITexture> loadTexture(
      const char* filename,
      bool calculateMipmapLevels = true,
      TextureFormat format = igl::TextureFormat::RGBA_SRGB,
      igl::TextureDesc::TextureUsageBits usage = igl::TextureDesc::TextureUsageBits::Sampled);

  std::shared_ptr<ITexture> loadTexture(
      const ImageData& imageData,
      bool calculateMipmapLevels = true,
      TextureFormat format = igl::TextureFormat::RGBA_SRGB,
      igl::TextureDesc::TextureUsageBits usage = igl::TextureDesc::TextureUsageBits::Sampled,
      const char* debugName = "");
  // 'argc' and 'argv' are the exact arguments received in 'main()'.
  static int argc();
  static char** argv();

  // Don't call this from the application level. The shell framework will use this API to expose
  // command line arguments for the application.
  static void initializeCommandLineArgs(int argc, char** argv);

 public:
  Extension* createAndInitializeExtension(const char* name) noexcept;

  /**
   * @brief Create a And Initialize object
   *
   * @return template <typename E>*
   */
  // E is the Extension type. Requirements on E
  // 1. The static method `const char* E::Name() noexcept` must exist
  // 2. E must subclass from igl::shell::Extension
  template<typename E>
  E* createAndInitialize() noexcept {
    // TODO static_assert() to enforce subclass
    return static_cast<E*>(createAndInitializeExtension(E::Name()));
  }

 private:
  struct State;
  std::unique_ptr<State> state_;
};

} // namespace igl::shell
