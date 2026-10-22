## [Download for Windows (x64)](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/releases/download/v0.1.0-alpha.10/N3-Recompiled-v0.1.0-alpha.10-windows-x64.zip)

Extract the whole ZIP and run **n3.exe**. Supply an ISO made from your own
supported USA/Europe disc, or your own extracted `default.xex` and `data` folder.
No game files, saves, profiles or developer logs are bundled. Existing local
setup and saves are reused.

### Physical-memory support across the existing native modules

Alpha 10 fixes address mapping in the remaining **27 memory-backed C++ entry
points**: geometry, vectors, memory primitives, delay/feedback audio and the frame
clock. High guest addresses use the pinned runtime's physical-memory shadow
offset. Range and alias checks preserve original behavior at boundaries and
overlaps. Countdown progress now retains the original path for stack aliases;
clock, box sampling and box-sphere results match its 32-bit saved-LR restoration.

All **43 memory-backed typed entries** now have mapped-address checks and
physical-memory fixtures. The other two entries use only scalar registers.
The project still has **45 typed native entry points**; this release strengthens
their address and calling-convention support. It does not convert the rest of
the game or remove unsupported-case translations. See the
[conversion plan](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/blob/v0.1.0-alpha.10/docs/CONVERSION-PLAN.md).

### Validation and limits

Both Windows configurations pass **40 test suites**. New physical fixtures add
**14,536 comparisons**: 8,680 native and 5,856 retained, plus 51 cached live-clock
changes. They cover unaligned and upper-page data, host offsets above 4 GiB,
individually high objects/buffers/stacks, boundary/wrap fallback, aliases,
history wrap, late audio failure and memory canaries. Existing virtual-address,
FP and retained-chain comparisons continue to pass.
**Eighteen portable component suites pass on Windows, Linux and Apple Silicon**;
the playable game remains **Windows x64 with Direct3D 12**.

Package member hashes, embedded icon, matching corresponding sources and private
data exclusions are checked. A fresh-profile Direct3D startup check runs the
extracted package. These are synthetic/startup checks, with no additional
gameplay coverage or performance claim.

The release retains live **30/40/60 FPS**, **1x–16x filtering**, display sizes
through **4K**, **720p/1440p/4K internal rendering**, and **VSync On/Off**.
40/60 FPS remain experimental; animations can appear accelerated. Later
missions/characters, a full playthrough and other PCs remain unvalidated.
Vulkan remains parked.

### Updating

Extract into a fresh application folder and keep `%LOCALAPPDATA%\N3Recompiled`,
which contains saves, settings, imported data and the remembered game path.
Update desktop shortcuts if the application folder changes; shortcuts made
from `n3.exe` inherit its embedded icon. No Python, Node.js or build tools are
needed to play.

### Developer sources — optional

[SDK, dependencies and renderer extensions](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/releases/download/v0.1.0-alpha.10/ReXGlue-bd833a2-corresponding-source.zip)
are provided for rebuilding and licensing; **they are not needed to play**.
The project's native modules are in the matching source tag.

[SHA-256 checksums](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/releases/download/v0.1.0-alpha.10/SHA256SUMS.txt)
cover both ZIPs. Third-party notices are included in the Windows package.
