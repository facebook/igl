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
  explicit ImGuiRenderer(igl::IDevice& device,
                         const char* defaultFontTTF = nullptr,
                         float fontSizePixels = 24.0f);
  ~ImGuiRenderer();

  void beginFrame(const igl::Framebuffer& desc);
  void endFrame(igl::IDevice& device, igl::ICommandBuffer& cmdBuffer);

 private:
  lvk::Holder<lvk::RenderPipelineHandle> createNewPipelineState(const igl::Framebuffer& desc);

 private:
  igl::IDevice& device_;
  lvk::Holder<lvk::RenderPipelineHandle> pipeline_;
  std::shared_ptr<igl::ITexture> fontTexture_;

  uint32_t frameIndex_ = 0;

  struct DrawableData {
    std::shared_ptr<igl::IBuffer> vb_;
    std::shared_ptr<igl::IBuffer> ib_;
    explicit DrawableData(igl::IDevice& device);
  };

  std::vector<DrawableData> drawables_[3] = {};
};

} // namespace lvk
