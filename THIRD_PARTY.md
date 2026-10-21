# Third-party notices

ReXGlue supplies generated project/build boilerplate and the downloaded runtime.
It is derived in part from Xenia. Its source and licence are available at:

- https://github.com/rexglue/rexglue-sdk/tree/bd833a2ab69d305f99ddf57418fc38ffec8af93d
- https://github.com/rexglue/rexglue-sdk/blob/bd833a2ab69d305f99ddf57418fc38ffec8af93d/LICENSE
- https://github.com/xenia-project/xenia

The project's disc extractor implements the XDVDFS structure documented by
Xenia's disc image reader. ReXGlue's own dependency notices remain applicable to
downloaded libraries and any future packaged binary distribution.

Bootstrap downloads official LLVM, CMake, Ninja, and Microsoft development tools.
Their respective licences apply. The pinned portable Microsoft-tool downloader
is by mmozeiko: https://gist.github.com/mmozeiko/7f3162ec2988e81e56d5c4e22cde9977.

LostOdysseyRecomp and UnleashedRecomp were consulted as architectural references;
their source is not included here. Original Ninety-Nine Nights material is not
included in the tracked project or release asset files.

## Binary releases

The first Windows alpha includes the unmodified ReXGlue runtime and Xenos GPU DLLs
from the pinned SDK. Alpha 2 and development builds retain that runtime and rebuild the Xenos
plugin with the live-resolution extension in `tools/gpu/`; `tools/prepare-gpu.py`
applies the extension to the pinned source without modifying the installed SDK.
Alpha 3 also builds a project presenter from the SDK's D3D12 presenter sources to
provide host VSync, retaining the shared SDK callback types and provider/runtime.
Its third-party components include SDL3, FFmpeg, libmspack,
fmt, spdlog, simde, xxHash, toml++, utfcpp, Dear ImGui, CLI11, o1heap, and small
vendored utility libraries. Their notices, including notices embedded in source
headers, are preserved under `licenses/` in the package. This software is based
in part on the work of the Independent JPEG Group.

FFmpeg and libmspack contain LGPL components. Each release supplies a separate
corresponding-source archive containing the SDK, its non-Vulkan dependencies at
their pinned commits, original build scripts, and license texts. The SDK's CMake
files specify the codec sources and configuration. `SOURCE-MANIFEST.json` in that
archive records upstream URLs, commit IDs, and source archive checksums.
Compatible rebuilt `rexruntime.dll` and `rexgpu-xenos.dll` may replace the supplied
DLLs. Reverse engineering for debugging modifications to LGPL components is
permitted. The N3 project source is available at the corresponding release tag;
building translated game code requires a matching executable from a disc.

Application-local Microsoft Visual C++ release x64 runtime DLLs are included from
the official Microsoft redistributable archive. Microsoft retains their rights;
see the package notice and [Microsoft's redistribution documentation](https://learn.microsoft.com/cpp/windows/redistributing-visual-cpp-files).
