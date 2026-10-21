// Developer probe using isolated fetch registers and the actual sampler path.
// No game registers, textures, saves or configuration files are modified.
bool D3D12TextureCache::AuditSamplerPolicy() {
  auto regs = std::make_unique<RegisterFile>();
  std::memset(regs->values, 0, sizeof(regs->values));
  D3D12TextureCache probe(*regs, static_cast<D3D12SharedMemory&>(shared_memory()),
      1, 1, command_processor_, bindless_resources_used_);
  D3D12Shader::SamplerBinding binding{};
  binding.mag_filter = binding.min_filter = binding.mip_filter = xenos::TextureFilter::kUseFetchConst;
  binding.aniso_filter = xenos::AnisoFilter::kUseFetchConst;
  const int requested = rex::cvar::Query<int32_t>("anisotropic_override");
  D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
  heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
  heap_desc.NumDescriptors = 1;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;
  if (FAILED(command_processor_.GetD3D12Provider().GetDevice()->CreateDescriptorHeap(
      &heap_desc, IID_PPV_ARGS(&heap)))) return false;
  bool ok = true;
  // Eligible trilinear texture, point-filtered texture, and a base-map texture.
  for (uint32_t variant = 0; variant < 3; ++variant) {
    xenos::xe_gpu_texture_fetch_t fetch{};
    fetch.type = xenos::FetchConstantType::kTexture;
    fetch.dimension = xenos::DataDimension::k2DOrStacked;
    fetch.size_2d.width = fetch.size_2d.height = 63;
    fetch.base_address = fetch.mip_address = 1;
    fetch.mip_max_level = 4;
    fetch.mag_filter = fetch.min_filter = variant == 1 ? xenos::TextureFilter::kPoint : xenos::TextureFilter::kLinear;
    fetch.mip_filter = variant == 2 ? xenos::TextureFilter::kBaseMap : xenos::TextureFilter::kLinear;
    fetch.aniso_filter = xenos::AnisoFilter::kMax_4_1;
    std::memcpy(&regs->values[XE_GPU_REG_SHADER_CONSTANT_FETCH_00_0], &fetch, sizeof(fetch));
    const auto parameters = probe.GetSamplerParameters(binding);
    const uint32_t expected = requested == 0 ? 0 : (variant == 0 && requested > 0 ? uint32_t(requested) : 3u);
    ok = ok && uint32_t(parameters.aniso_filter) == expected;
    probe.WriteSampler(parameters, heap->GetCPUDescriptorHandleForHeapStart());
  }
  REXGPU_INFO("Graphics sampler probe: override {} accepted {}", requested, ok);
  return ok;
}
