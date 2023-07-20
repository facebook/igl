/*
* LightweightVK
*
* This source code is licensed under the MIT license found in the
* LICENSE file in the root directory of this source tree.
*/

#pragma once

#include <vector>

#include <lvk/LVK.h>
#include <imgui/imgui.h>

namespace lvk {

class ImGuiRenderer {
 public:
  explicit ImGuiRenderer(lvk::IDevice& device,
                         const char* defaultFontTTF = nullptr,
                         float fontSizePixels = 24.0f);
  ~ImGuiRenderer();

  void beginFrame(const lvk::Framebuffer& desc);
  void endFrame(lvk::IDevice& device, lvk::ICommandBuffer& cmdBuffer);

 private:
  lvk::Holder<lvk::RenderPipelineHandle> createNewPipelineState(const lvk::Framebuffer& desc);

 private:
  lvk::IDevice& device_;
  lvk::Holder<lvk::RenderPipelineHandle> pipeline_;
  std::shared_ptr<lvk::ITexture> fontTexture_;

  uint32_t frameIndex_ = 0;

  struct DrawableData {
    std::shared_ptr<lvk::IBuffer> vb_;
    std::shared_ptr<lvk::IBuffer> ib_;
    explicit DrawableData(lvk::IDevice& device);
  };

  std::vector<DrawableData> drawables_[3] = {};
};

} // namespace lvk
