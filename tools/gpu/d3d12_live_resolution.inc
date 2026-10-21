// N3 extension to the pinned ReXGlue D3D12 command processor.
// Called on the GPU worker after the completed swap submission. Graphics is a
// main-menu page: its next frame redraws the discarded render-target contents.
void D3D12CommandProcessor::ApplyN3DrawResolutionScale() {
  if (rex::cvar::Query<bool>("graphics_audit")) {
    static int previous_filter = -99;
    const auto filter = rex::cvar::Query<int32_t>("anisotropic_override");
    if (filter != previous_filter) {
      previous_filter = filter;
      if (!texture_cache_->AuditSamplerPolicy()) REXGPU_ERROR("Graphics sampler probe failed");
    }
  }
  const uint32_t previous = texture_cache_->draw_resolution_scale_x();
  REXCVAR_SET(n3_render_scale_active, previous);
  const uint32_t requested = REXCVAR_GET(n3_live_resolution_request);
  if (!requested || requested == previous) return;
  const auto reject = [previous] {
    for (const auto* flag : {"draw_resolution_scale_x", "draw_resolution_scale_y", "resolution_scale"})
      rex::cvar::SetFlagByName(flag, std::to_string(previous));
    REXCVAR_SET(n3_live_resolution_request, previous);
    REXCVAR_SET(n3_live_resolution_failed, true);
  };
  uint32_t x = requested, y = requested;
  if (!D3D12TextureCache::ClampDrawResolutionScaleToMaxSupported(x, y, GetD3D12Provider()) ||
      x != requested || y != requested) {
    REXGPU_WARN("Live render scale {}x is unsupported; retaining {}x", requested, previous);
    reject();
    return;
  }
  if (submission_open_ || frame_open_ || !AwaitAllQueueOperationsCompletion()) return;

  // Keep guest memory, GPU shared memory, registers, command-ring state, and the
  // presenter alive. Only scale-dependent resources and shader translations are
  // replaced. Retain the old resources until all replacement allocation works.
  auto targets = std::make_unique<D3D12RenderTargetCache>(
      *register_file_, *memory_, x, y, *this, bindless_resources_used_);
  if (!targets->Initialize() || targets->GetPath() != render_target_cache_->GetPath()) {
    REXGPU_WARN("Live render target allocation failed; retaining {}x", previous);
    reject();
    return;
  }
  auto textures = D3D12TextureCache::Create(
      *register_file_, *shared_memory_, x, y, *this, bindless_resources_used_);
  if (!textures) {
    REXGPU_WARN("Live texture cache allocation failed; retaining {}x", previous);
    reject();
    return;
  }
  auto pipelines = std::make_unique<PipelineCache>(
      *this, *register_file_, *targets, bindless_resources_used_);
  if (!pipelines->Initialize()) {
    REXGPU_WARN("Live pipeline allocation failed; retaining {}x", previous);
    reject();
    return;
  }
  auto reload_shader = [&pipelines](Shader* old) -> Shader* {
    if (!old) return nullptr;
    auto code = old->ucode_data();
    for (auto& word : code) word = rex::byte_swap(word);
    return pipelines->LoadShader(old->type(), code.data(), uint32_t(code.size()));
  };
  Shader* vertex = reload_shader(active_vertex_shader_);
  Shader* pixel = reload_shader(active_pixel_shader_);
  // The old pipeline cache owns its shaders and persistent-storage writer.
  pipeline_cache_.reset();
  texture_cache_.reset();
  render_target_cache_.reset();
  render_target_cache_ = std::move(targets);
  texture_cache_ = std::move(textures);
  pipeline_cache_ = std::move(pipelines);
  active_vertex_shader_ = vertex;
  active_pixel_shader_ = pixel;
  if (!n3_shader_cache_root_.empty())
    pipeline_cache_->InitializeShaderStorage(n3_shader_cache_root_, n3_shader_title_id_, false);

  // The heap stays allocated, but its EDRAM descriptors must name the new buffer.
  if (bindless_resources_used_) {
    const auto& provider = GetD3D12Provider();
    render_target_cache_->WriteEdramRawSRVDescriptor(provider.OffsetViewDescriptor(
        view_bindless_heap_cpu_start_, uint32_t(SystemBindlessView::kEdramRawSRV)));
    render_target_cache_->WriteEdramRawUAVDescriptor(provider.OffsetViewDescriptor(
        view_bindless_heap_cpu_start_, uint32_t(SystemBindlessView::kEdramRawUAV)));
    for (uint32_t power = 2; power <= 4; ++power) {
      render_target_cache_->WriteEdramUintPow2SRVDescriptor(provider.OffsetViewDescriptor(
          view_bindless_heap_cpu_start_, uint32_t(SystemBindlessView::kEdramR32UintSRV) + power - 2), power);
      render_target_cache_->WriteEdramUintPow2UAVDescriptor(provider.OffsetViewDescriptor(
          view_bindless_heap_cpu_start_, uint32_t(SystemBindlessView::kEdramR32UintUAV) + power - 2), power);
    }
  }
  current_texture_layout_uid_vertex_ = current_texture_layout_uid_pixel_ = SIZE_MAX;
  current_sampler_layout_uid_vertex_ = current_sampler_layout_uid_pixel_ = SIZE_MAX;
  cbuffer_binding_descriptor_indices_vertex_.up_to_date = false;
  cbuffer_binding_descriptor_indices_pixel_.up_to_date = false;
  bindful_textures_written_vertex_ = bindful_textures_written_pixel_ = false;
  bindful_samplers_written_vertex_ = bindful_samplers_written_pixel_ = false;
  InvalidateAllVertexBufferResidency();
  REXCVAR_SET(n3_render_scale_active, requested);
  REXCVAR_SET(n3_live_resolution_failed, false);
  REXGPU_INFO("Live render resolution applied: {}x{} ({}x{})", x, y, 1280 * x, 720 * y);
}
