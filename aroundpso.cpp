  }
  IGL_LOG_INFO("  Root signature serialized OK\n");

  IGL_LOG_INFO("  Creating root signature...\n");
  Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
  hr = device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                    IID_PPV_ARGS(rootSignature.GetAddressOf()));
  if (FAILED(hr)) {
    IGL_LOG_ERROR("  CreateRootSignature FAILED: 0x%08X\n", static_cast<unsigned>(hr));
    Result::setResult(outResult, Result::Code::RuntimeError, "Failed to create root signature");
    return nullptr;
  }
  IGL_LOG_INFO("  Root signature created OK\n");

  // Create PSO - zero-initialize all fields
  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
  psoDesc.pRootSignature = rootSignature.Get();

  // Shader bytecode
  psoDesc.VS = {vsBytecode.data(), vsBytecode.size()};
  psoDesc.PS = {psBytecode.data(), psBytecode.size()};
  // Explicitly zero unused shader stages
  psoDesc.DS = {nullptr, 0};
  psoDesc.HS = {nullptr, 0};
  psoDesc.GS = {nullptr, 0};

  // Rasterizer state - D3D12 default values
  psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
  psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;  // Disable culling for debugging
  psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
  psoDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
  psoDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
  psoDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
  psoDesc.RasterizerState.DepthClipEnable = TRUE;
  psoDesc.RasterizerState.MultisampleEnable = FALSE;
  psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
  psoDesc.RasterizerState.ForcedSampleCount = 0;
  psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

  // Blend state - D3D12 default values (all RT blend disabled)
  psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
  psoDesc.BlendState.IndependentBlendEnable = FALSE;
  for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i) {
    psoDesc.BlendState.RenderTarget[i].BlendEnable = FALSE;
    psoDesc.BlendState.RenderTarget[i].LogicOpEnable = FALSE;
    psoDesc.BlendState.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;
    psoDesc.BlendState.RenderTarget[i].DestBlend = D3D12_BLEND_ZERO;
    psoDesc.BlendState.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
    psoDesc.BlendState.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
    psoDesc.BlendState.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ZERO;
    psoDesc.BlendState.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    psoDesc.BlendState.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_NOOP;
    psoDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
  }

  // Depth stencil state
  if (desc.targetDesc.depthAttachmentFormat != TextureFormat::Invalid) {
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
  } else {
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
  }

  // Render target format (must match swapchain format!)
  psoDesc.NumRenderTargets = 1;
  psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;  // Match swapchain format, not SRGB
  if (desc.targetDesc.depthAttachmentFormat != TextureFormat::Invalid) {
    psoDesc.DSVFormat = textureFormatToDXGIFormat(desc.targetDesc.depthAttachmentFormat);
  } else {
    psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
  }

  // Sample settings
  psoDesc.SampleMask = UINT_MAX;
  psoDesc.SampleDesc.Count = 1;
  psoDesc.SampleDesc.Quality = 0;  // Must be 0 for Count=1

  // Primitive topology
  psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  psoDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

  // Additional required fields
  psoDesc.NodeMask = 0;  // Single GPU operation
  psoDesc.CachedPSO.pCachedBlob = nullptr;
  psoDesc.CachedPSO.CachedBlobSizeInBytes = 0;
  psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

  // Input layout (Phase 3 Step 3.4)
  std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements;
  std::vector<std::string> semanticNames; // Keep semantic name strings alive

  if (desc.vertexInputState) {
    // Convert IGL vertex input state to D3D12 input layout
    auto* d3d12VertexInput = static_cast<const VertexInputState*>(desc.vertexInputState.get());
    const auto& vertexDesc = d3d12VertexInput->getDesc();

    // Pre-reserve space to prevent reallocation (which would invalidate c_str() pointers)
    semanticNames.reserve(vertexDesc.numAttributes);
