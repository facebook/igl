/*
* LightweightVK
*
* This source code is licensed under the MIT license found in the
* LICENSE file in the root directory of this source tree.
*/

#include "HelpersImGui.h"

#include <math.h>

static const char* codeVS = R"(
layout(location = 0) in vec2 in_pos;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec4 in_color;

layout (location = 0) out vec4 out_color;
layout (location = 1) out vec2 out_uv;
layout (location = 2) out flat uint out_textureId;

layout(push_constant) uniform PushConstants {
    vec4 LRTB;
    uint textureId;
} pc;

void main() {
  float L = pc.LRTB.x;
  float R = pc.LRTB.y;
  float T = pc.LRTB.z;
  float B = pc.LRTB.w;
  mat4 proj = mat4(
    2.0 / (R - L),                   0.0,  0.0, 0.0,
    0.0,                   2.0 / (T - B),  0.0, 0.0,
    0.0,                             0.0, -1.0, 0.0,
    (R + L) / (L - R), (T + B) / (B - T),  0.0, 1.0);
  out_color = in_color;
  out_uv = in_uv;
  out_textureId = pc.textureId;
  gl_Position = proj * vec4(in_pos, 0, 1);
})";

static const char* codeFS = R"(
layout (location = 0) in vec4 in_color;
layout (location = 1) in vec2 in_uv;
layout (location = 2) in flat uint in_textureId;

layout (location = 0) out vec4 out_color;

void main() {
  out_color = in_color * texture(sampler2D(kTextures2D[in_textureId], kSamplers[0]), in_uv);
})";

namespace lvk {

ImGuiRenderer::DrawableData::DrawableData(igl::IDevice& device) {
  IGL_ASSERT_MSG(sizeof(ImDrawIdx) == 2, "The constants below may not work with the ImGui data.");
  const size_t kMaxVertices = 65536u;
  const size_t kMaxVertexBufferSize = kMaxVertices * sizeof(ImDrawVert);
  const size_t kMaxIndexBufferSize = kMaxVertices * sizeof(ImDrawIdx);

  const igl::BufferDesc vbDesc = {.usage = igl::BufferUsageBits_Vertex,
                                  .storage = igl::StorageType_HostVisible,
                                  .size = kMaxVertexBufferSize};
  const igl::BufferDesc ibDesc = {.usage = igl::BufferUsageBits_Index,
                                  .storage = igl::StorageType_HostVisible,
                                  .size = kMaxIndexBufferSize};
  vb_ = device.createBuffer(vbDesc, nullptr);
  ib_ = device.createBuffer(ibDesc, nullptr);
}

lvk::Holder<lvk::RenderPipelineHandle> ImGuiRenderer::createNewPipelineState(const igl::Framebuffer& desc) {
  return device_.createRenderPipeline(
      {
          .vertexInput = {.attributes = {{.location = 0,
                                          .format = igl::VertexFormat::Float2,
                                          .offset = offsetof(ImDrawVert, pos)},
                                         {.location = 1,
                                          .format = igl::VertexFormat::Float2,
                                          .offset = offsetof(ImDrawVert, uv)},
                                         {.location = 2,
                                          .format = igl::VertexFormat::UByte4Norm,
                                          .offset = offsetof(ImDrawVert, col)}},
                          .inputBindings = {{.stride = sizeof(ImDrawVert)}}},
          .shaderStages = device_.createShaderStages(codeVS,
                                                     "Shader Module: imgui (vert)",
                                                     codeFS,
                                                     "Shader Module: imgui (frag)",
                                                     nullptr),
          .colorAttachments = {{
              .textureFormat = desc.colorAttachments[0].texture->getFormat(),
              .blendEnabled = true,
              .srcRGBBlendFactor = igl::BlendFactor_SrcAlpha,
              .dstRGBBlendFactor = igl::BlendFactor_OneMinusSrcAlpha,
          }},
          .depthAttachmentFormat = desc.depthStencilAttachment.texture
                                       ? desc.depthStencilAttachment.texture->getFormat()
                                       : igl::TextureFormat::Invalid,
          .cullMode = igl::CullMode_None,
      },
      nullptr);
}

ImGuiRenderer::ImGuiRenderer(igl::IDevice& device, const char* defaultFontTTF, float fontSizePixels) : device_(device) {
  ImGui::CreateContext();

  ImGuiIO& io = ImGui::GetIO();

  ImFontConfig cfg = ImFontConfig();
  cfg.FontDataOwnedByAtlas = false;
  cfg.RasterizerMultiply = 1.5f;
  cfg.SizePixels = ceilf(fontSizePixels);
  cfg.PixelSnapH = true;
  cfg.OversampleH = 4;
  cfg.OversampleV = 4;
  ImFont* font = nullptr;
  if (defaultFontTTF) {
    font = io.Fonts->AddFontFromFileTTF(defaultFontTTF, cfg.SizePixels, &cfg);
  }

  io.Fonts->Flags |= ImFontAtlasFlags_NoPowerOfTwoHeight;

  // init fonts
  unsigned char* pixels;
  int width, height;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  fontTexture_ = device.createTexture({.type = igl::TextureType::TwoD,
                                       .format = igl::TextureFormat::RGBA_UN8,
                                       .width = (uint32_t)width,
                                       .height = (uint32_t)height,
                                       .usage = igl::TextureUsageBits_Sampled},
                                      nullptr);
  const void* data[] = {pixels};
  fontTexture_->upload({.width = (uint32_t)width, .height = (uint32_t)height}, data);

  io.BackendRendererName = "imgui-igl-next";
  io.Fonts->TexID = ImTextureID(fontTexture_.get());
  io.FontDefault = font;
}

ImGuiRenderer::~ImGuiRenderer() {
  ImGuiIO& io = ImGui::GetIO();
  io.Fonts->TexID = nullptr;
  ImGui::DestroyContext();
}

void ImGuiRenderer::beginFrame(const igl::Framebuffer& desc) {
  const float displayScale = 1.0f;

  const igl::Dimensions dim = desc.colorAttachments[0].texture->getDimensions();

  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(dim.width / displayScale, dim.height / displayScale);
  io.DisplayFramebufferScale = ImVec2(displayScale, displayScale);
  io.IniFilename = nullptr;

  if (pipeline_.empty()) {
    pipeline_ = createNewPipelineState(desc);
  }
  ImGui::NewFrame();
}

void ImGuiRenderer::endFrame(igl::IDevice& device, igl::ICommandBuffer& cmdBuffer) {
  ImGui::EndFrame();
  ImGui::Render();

  ImDrawData* drawData = ImGui::GetDrawData();

  int fb_width = (int)(drawData->DisplaySize.x * drawData->FramebufferScale.x);
  int fb_height = (int)(drawData->DisplaySize.y * drawData->FramebufferScale.y);
  if (fb_width <= 0 || fb_height <= 0 || drawData->CmdListsCount == 0) {
    return;
  }

  cmdBuffer.cmdBindViewport({
      .x = 0.0f,
      .y = 0.0f,
      .width = (drawData->DisplaySize.x * drawData->FramebufferScale.x),
      .height = (drawData->DisplaySize.y * drawData->FramebufferScale.y),
  });

  const float L = drawData->DisplayPos.x;
  const float R = drawData->DisplayPos.x + drawData->DisplaySize.x;
  const float T = drawData->DisplayPos.y;
  const float B = drawData->DisplayPos.y + drawData->DisplaySize.y;

  struct VulkanImguiBindData {
    float LRTB[4]; // ortho projection: left, right, top, bottom
    uint32_t textureId = 0;
  } bindData = {
      .LRTB = {L, R, T, B},
  };

  const ImVec2 clip_off = drawData->DisplayPos;
  const ImVec2 clip_scale = drawData->FramebufferScale;

  std::vector<DrawableData>& curFrameDrawables = drawables_[frameIndex_];
  frameIndex_ = (frameIndex_ + 1) % LVK_ARRAY_NUM_ELEMENTS(drawables_);

  cmdBuffer.cmdPushConstants(&bindData, sizeof(bindData));

  ImTextureID lastBoundTextureId = nullptr;

  for (int n = 0; n < drawData->CmdListsCount; n++) {
    const ImDrawList* cmd_list = drawData->CmdLists[n];

    if (n >= curFrameDrawables.size()) {
      curFrameDrawables.emplace_back(device);
    }
    DrawableData& drawableData = curFrameDrawables[n];

    // Upload vertex/index buffers
    drawableData.vb_->upload(cmd_list->VtxBuffer.Data,
                             cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
    drawableData.ib_->upload(cmd_list->IdxBuffer.Data,
                             cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));

    for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
      const ImDrawCmd cmd = cmd_list->CmdBuffer[cmd_i];
      IGL_ASSERT(cmd.UserCallback == nullptr);

      const ImVec2 clipMin((cmd.ClipRect.x - clip_off.x) * clip_scale.x,
                           (cmd.ClipRect.y - clip_off.y) * clip_scale.y);
      const ImVec2 clipMax((cmd.ClipRect.z - clip_off.x) * clip_scale.x,
                           (cmd.ClipRect.w - clip_off.y) * clip_scale.y);

      if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y) {
        continue;
      }

      const igl::ScissorRect rect{uint32_t(clipMin.x),
                                  uint32_t(clipMin.y),
                                  uint32_t(clipMax.x - clipMin.x),
                                  uint32_t(clipMax.y - clipMin.y)};
      cmdBuffer.cmdBindScissorRect(rect);

      if (cmd.TextureId != lastBoundTextureId) {
        lastBoundTextureId = cmd.TextureId;
        bindData.textureId = cmd.TextureId ? reinterpret_cast<igl::ITexture*>(cmd.TextureId)->getTextureId() : 0u;
        cmdBuffer.cmdPushConstants(bindData);
      }

      cmdBuffer.cmdBindRenderPipeline(pipeline_);
      cmdBuffer.cmdBindVertexBuffer(0, drawableData.vb_, 0);
      cmdBuffer.cmdDrawIndexed(igl::PrimitiveType::Triangle,
                               cmd.ElemCount,
                               sizeof(ImDrawIdx) == sizeof(uint16_t) ? igl::IndexFormat::UInt16
                                                                     : igl::IndexFormat::UInt32,
                               *drawableData.ib_,
                               cmd.IdxOffset * sizeof(ImDrawIdx));
    }
  }
}

} // namespace lvk
