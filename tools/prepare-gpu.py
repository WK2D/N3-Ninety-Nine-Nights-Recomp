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
fingerprint = hashlib.sha256(Path(__file__).read_bytes() + extension.read_bytes() +
    str(archive.stat().st_mtime_ns).encode()).hexdigest()
marker = source / '.n3-extension'
if marker.is_file() and marker.read_text() == fingerprint:
    print('Pinned GPU extension is up to date')
    raise SystemExit(0)
with zipfile.ZipFile(archive) as z:
    # Refresh only files modified by the extension when already extracted.
    names = ['src/graphics/CMakeLists.txt', 'src/graphics/d3d12/command_processor.cpp',
             'include/rex/graphics/d3d12/command_processor.h']
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
marker.write_text(fingerprint)
print('Pinned GPU source prepared with live-resolution extension')
