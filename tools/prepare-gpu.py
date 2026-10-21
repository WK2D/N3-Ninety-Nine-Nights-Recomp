"""Prepare the pinned SDK plugin source and apply the local renderer extension."""
from pathlib import Path
import hashlib
import zipfile

ROOT = Path(__file__).resolve().parent.parent
source = ROOT / '.tools/live-gpu-source/rexglue-sdk'
archive = ROOT / 'out/release/ReXGlue-bd833a2-corresponding-source.zip'
if not archive.is_file():
    raise SystemExit('Run tools/bundle-sdk-source.py to obtain the pinned SDK sources first.')
extension = ROOT / 'tools/gpu/d3d12_live_resolution.inc'
fingerprint = hashlib.sha256(Path(__file__).read_bytes() + b''.join(p.read_bytes() for p in sorted((ROOT / 'tools/gpu').glob('*.inc'))) +
    str(archive.stat().st_mtime_ns).encode()).hexdigest()
marker = source / '.n3-extension'
if marker.is_file() and marker.read_text() == fingerprint:
    print('Pinned GPU extension is up to date')
    raise SystemExit(0)
with zipfile.ZipFile(archive) as z:
    # Refresh only files modified by the extension when already extracted.
    names = ['src/graphics/CMakeLists.txt', 'src/graphics/d3d12/command_processor.cpp',
             'include/rex/graphics/d3d12/command_processor.h', 'src/graphics/graphics_system.cpp',
             'src/graphics/d3d12/texture_cache.cpp', 'include/rex/graphics/d3d12/texture_cache.h']
    if not (source / 'src/graphics/plugin_main.cpp').is_file():
        names = [n.removeprefix('rexglue-sdk/') for n in z.namelist()
                 if n.startswith('rexglue-sdk/') and not n.endswith('/')]
    for name in names:
        if '..' in Path(name).parts or Path(name).is_absolute():
            raise RuntimeError('Unsafe SDK source path')
        target = source / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(z.read('rexglue-sdk/' + name))

def replace(name, before, after):
    file = source / name
    text = file.read_text(encoding='utf-8')
    if text.count(before) != 1:
        raise RuntimeError('Pinned GPU source does not match extension: ' + name)
    file.write_text(text.replace(before, after), encoding='utf-8', newline='\n')

replace('src/graphics/CMakeLists.txt', 'add_library(rex::gpu-xenos ALIAS rexgpu-xenos)', '')
replace('include/rex/graphics/d3d12/command_processor.h',
        '  static constexpr uint32_t kQueueFrames = 3;',
        '  void ApplyN3DrawResolutionScale();\n  std::filesystem::path n3_shader_cache_root_;\n'
        '  uint32_t n3_shader_title_id_ = 0;\n  static constexpr uint32_t kQueueFrames = 3;')
replace('src/graphics/d3d12/command_processor.cpp',
        '  pipeline_cache_->InitializeShaderStorage(cache_root, title_id, blocking);',
        '  n3_shader_cache_root_ = cache_root;\n  n3_shader_title_id_ = title_id;\n'
        '  pipeline_cache_->InitializeShaderStorage(cache_root, title_id, blocking);')
replace('src/graphics/d3d12/command_processor.cpp',
        '  EndSubmission(true);\n}\n\nvoid D3D12CommandProcessor::OnPrimaryBufferEnd()',
        '  EndSubmission(true);\n  ApplyN3DrawResolutionScale();\n}\n\n#include "n3_live_resolution.inc"\n\nvoid D3D12CommandProcessor::OnPrimaryBufferEnd()')
replace('src/graphics/d3d12/command_processor.cpp',
        'bool D3D12CommandProcessor::SetupContext() {',
        'REXCVAR_DEFINE_UINT32(n3_live_resolution_request, 0, "N3/Graphics", "Live render scale request").range(0, 3).debug_only();\n'
        'REXCVAR_DEFINE_BOOL(n3_live_resolution_failed, false, "N3/Graphics", "Live render scale failure").debug_only();\n'
        'REXCVAR_DEFINE_UINT32(n3_render_scale_active, 0, "N3/Graphics", "Active render scale").debug_only();\n\n'
        'bool D3D12CommandProcessor::SetupContext() {')
(source / 'src/graphics/d3d12/n3_live_resolution.inc').write_bytes(
    extension.read_bytes())
# Use a project presenter while retaining the SDK's shared callback/context types.
# The runtime's original presenter always passes SyncInterval=0.
with zipfile.ZipFile(archive) as z:
    header = z.read('rexglue-sdk/include/rex/ui/d3d12/d3d12_presenter.h').decode('utf-8')
    begin = header.index('class D3D12UIDrawContext final')
    end = header.index('class D3D12Presenter final', begin)
    header = header[:begin] + header[end:]
    begin = header.index('  class D3D12GuestOutputRefreshContext final')
    end = header.index('  static std::unique_ptr<D3D12Presenter> Create', begin)
    header = header[:begin] + '  using D3D12GuestOutputRefreshContext = D3D12Presenter::D3D12GuestOutputRefreshContext;\n\n' + header[end:]
    header = header.replace('D3D12Presenter', 'N3D3D12Presenter')
    header = header.replace('= N3D3D12Presenter::D3D12GuestOutputRefreshContext', '= D3D12Presenter::D3D12GuestOutputRefreshContext')
    header = header.replace('#pragma once', '#pragma once\n#include <rex/ui/d3d12/d3d12_presenter.h>')
    header = header.replace('    Microsoft::WRL::ComPtr<IDXGISwapChain3> swap_chain;',
        '    Microsoft::WRL::ComPtr<IDXGISwapChain3> swap_chain;\n    HANDLE frame_ready = nullptr;')
    presenter = z.read('rexglue-sdk/src/ui/d3d12/d3d12_presenter.cpp').decode('utf-8')
    presenter = presenter.replace('<rex/ui/d3d12/d3d12_presenter.h>', '<rex/ui/d3d12/n3_d3d12_presenter.h>')
    presenter = presenter.replace('D3D12Presenter', 'N3D3D12Presenter')
    begin = presenter.index('REXCVAR_DEFINE_BOOL(d3d12_allow_variable_refresh_rate_and_tearing')
    end = presenter.index('namespace rex::ui::d3d12', begin)
    presenter = presenter[:begin] + presenter[end:]
    presenter = presenter.replace('REXCVAR_GET(d3d12_allow_variable_refresh_rate_and_tearing)', 'rex::cvar::Query<bool>("d3d12_allow_variable_refresh_rate_and_tearing")')
    before = '''  HRESULT present_result = paint_context_.swap_chain->Present(
      0, DXGI_PRESENT_RESTART |
             (paint_context_.swap_chain_allows_tearing ? DXGI_PRESENT_ALLOW_TEARING : 0));'''
    after = '''  const bool sync = rex::cvar::Query<bool>("n3_vsync");
  const auto policy = n3::graphics::HostPresentPolicy(sync, paint_context_.swap_chain_allows_tearing);
  const UINT interval = policy.interval, flags = policy.flags;
  if (rex::cvar::Query<bool>("graphics_trace")) {
    static int previous = -1;
    if (previous != int(sync)) {
      previous = int(sync);
      REXLOG_INFO("Graphics host present: VSync {} interval {} flags {}", sync, interval, flags);
    }
  }
  HRESULT present_result = paint_context_.swap_chain->Present(interval, flags);'''
    if presenter.count(before) != 1:
        raise RuntimeError('Pinned presenter no longer matches VSync extension')
    presenter = presenter.replace(before, after)
    presenter = presenter.replace('#include <algorithm>', '#include <algorithm>\n#include "graphics_policy.h"')
    presenter = presenter.replace('    swap_chain_desc.Flags = 0;',
        '    swap_chain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;')
    presenter = presenter.replace('        paint_context_.swap_chain_allows_tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0));',
        '        DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT |\n'
        '        (paint_context_.swap_chain_allows_tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0)));')
    presenter = presenter.replace('    paint_context_.swap_chain_width = new_swap_chain_width;',
        '    paint_context_.swap_chain->SetMaximumFrameLatency(1);\n'
        '    if (!paint_context_.frame_ready)\n'
        '      paint_context_.frame_ready = paint_context_.swap_chain->GetFrameLatencyWaitableObject();\n'
        '    paint_context_.swap_chain_width = new_swap_chain_width;')
    presenter = presenter.replace('  swap_chain.Reset();',
        '  if (frame_ready) { CloseHandle(frame_ready); frame_ready = nullptr; }\n  swap_chain.Reset();')
    presenter = presenter.replace('Presenter::PaintResult N3D3D12Presenter::PaintAndPresentImpl(bool execute_ui_drawers) {',
        '''Presenter::PaintResult N3D3D12Presenter::PaintAndPresentImpl(bool execute_ui_drawers) {
  // Poll rather than waiting while the UI thread owns the presenter lock.
  // The next UI tick or guest frame retries with the latest image and input.
  if (rex::cvar::Query<bool>("n3_vsync") && paint_context_.frame_ready &&
      WaitForSingleObject(paint_context_.frame_ready, 0) != WAIT_OBJECT_0)
    return PaintResult::kNotPresented;''')
    # The copied explanation described the original forced immediate policy.
    begin = presenter.index('  // Present as soon as possible, without waiting for vsync')
    end = presenter.index('  const bool sync', begin)
    presenter = presenter[:begin] + '  // Host VSync is independent of guest vblank timing.\n' + presenter[end:]
    (source / 'include/rex/ui/d3d12/n3_d3d12_presenter.h').write_text(header, encoding='utf-8')
    (source / 'src/ui/d3d12/n3_d3d12_presenter.cpp').write_text(presenter, encoding='utf-8')
replace('src/graphics/graphics_system.cpp', '#include <rex/graphics/graphics_system.h>',
        '#include <rex/graphics/graphics_system.h>\n#include <rex/ui/d3d12/n3_d3d12_presenter.h>')
file = source / 'src/graphics/graphics_system.cpp'
text = file.read_text(encoding='utf-8')
old = 'provider_->CreatePresenter(loss_cb)'
if text.count(old) != 2: raise RuntimeError('Pinned presenter factory changed')
file.write_text(text.replace(old, 'ui::d3d12::N3D3D12Presenter::Create(loss_cb, *static_cast<ui::d3d12::D3D12Provider*>(provider_.get()))'), encoding='utf-8')
replace('src/graphics/d3d12/texture_cache.cpp',
        'D3D12TextureCache::~D3D12TextureCache() {',
        '#include "n3_sampler_audit.inc"\n\nD3D12TextureCache::~D3D12TextureCache() {')
replace('include/rex/graphics/d3d12/texture_cache.h',
        '  void ClearCache() override;', '  bool AuditSamplerPolicy();\n  void ClearCache() override;')
(source / 'src/graphics/d3d12/n3_sampler_audit.inc').write_bytes((ROOT / 'tools/gpu/d3d12_sampler_audit.inc').read_bytes())
replace('src/graphics/d3d12/texture_cache.cpp',
        '  if (anisotropic_override > -1 && anisotropic_override < 6 && has_mips && !mip_base_map &&',
        '  if (anisotropic_override == 0) aniso_filter = xenos::AnisoFilter::kDisabled;\n'
        '  else if (anisotropic_override > 0 && anisotropic_override < 6 && has_mips && !mip_base_map &&')
replace('src/graphics/d3d12/texture_cache.cpp',
        '  static const D3D12_TEXTURE_ADDRESS_MODE kAddressModeMap[] = {',
        '''  if (rex::cvar::Query<bool>("graphics_trace")) {
    static bool seen[7][17] = {};
    const int requested = std::clamp(rex::cvar::Query<int32_t>("anisotropic_override"), -1, 5);
    if (!seen[requested + 1][desc.MaxAnisotropy]) {
      seen[requested + 1][desc.MaxAnisotropy] = true;
      REXGPU_INFO("Graphics sampler: override {} filter {} actual anisotropy {}", requested, UINT(desc.Filter), desc.MaxAnisotropy);
    }
  }
  static const D3D12_TEXTURE_ADDRESS_MODE kAddressModeMap[] = {''')
replace('src/graphics/d3d12/command_processor.cpp',
        '  system::X_VIDEO_MODE video_mode;\n  kernel::xboxkrnl::VdQueryVideoMode(&video_mode);',
        '''  if (rex::cvar::Query<bool>("graphics_trace")) {
    static uint64_t previous = 0;
    const uint64_t dimensions = (uint64_t(guest_output_width) << 32) | guest_output_height;
    if (previous != dimensions) {
      previous = dimensions;
      REXGPU_INFO("Graphics swap source: {}x{} scaled {} cache {}x{}", guest_output_width, guest_output_height,
          swap_source_scaled, texture_cache_->draw_resolution_scale_x(), texture_cache_->draw_resolution_scale_y());
    }
  }
  system::X_VIDEO_MODE video_mode;
  kernel::xboxkrnl::VdQueryVideoMode(&video_mode);''')
marker.write_text(fingerprint)
print('Pinned GPU source prepared with live-resolution extension')
