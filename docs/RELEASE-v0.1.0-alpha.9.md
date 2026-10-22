## [Download for Windows (x64)](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/releases/download/v0.1.0-alpha.9/N3-Recompiled-v0.1.0-alpha.9-windows-x64.zip)

Extract the whole ZIP and run **n3.exe**. Supply an ISO made from your own
supported USA/Europe disc, or your own extracted `default.xex` and `data` folder.
No game files, saves, profiles or developer logs are bundled. Existing local
setup and saves are reused.

### Correct physical-memory access in native state operations

Alpha 9 fixes address mapping in **thirteen existing C++ entry points**:
bounded/table values, percentage changes, capped accounting, handle lookup and
validation, payload copy and related-object state operations. High guest
addresses now use the pinned runtime's physical-memory shadow offset for all
word and byte reads/writes. Pointer traversal, stack homes and alias checks use
the same mapping. Unsafe boundary/wrap cases retain the original translations.

The project still has **45 typed native entry points**. This release expands
their validated address support. Physical-memory comparisons now cover sixteen
entries, including the three direction routines from Alpha 8. Twenty-seven
older memory-backed entries still need the same audit. The
[conversion plan](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/blob/v0.1.0-alpha.9/docs/CONVERSION-PLAN.md)
tracks that work; full game-code conversion remains in progress.

### Validation and limits

Both Windows configurations pass **40 test suites**. The connected state suites
add **3,840 high-address comparisons**: 2,712 native and 1,128 retained cases.
They cover high objects/tables/records/payloads/stacks, category and enable bytes,
counter carry, signed wide products, unaligned and top-page data, pointer aliases,
boundary crossings and independent canaries for memory that must remain unchanged.
Existing virtual-address, retained-chain and FP checks continue to pass.
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

[SDK, dependencies and renderer extensions](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/releases/download/v0.1.0-alpha.9/ReXGlue-bd833a2-corresponding-source.zip)
are provided for rebuilding and licensing; **they are not needed to play**.
The project's native modules are in the matching source tag.

[SHA-256 checksums](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/releases/download/v0.1.0-alpha.9/SHA256SUMS.txt)
cover both ZIPs. Third-party notices are included in the Windows package.
