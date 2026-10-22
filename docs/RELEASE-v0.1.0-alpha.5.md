## [Download for Windows (x64)](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/releases/download/v0.1.0-alpha.5/N3-Recompiled-v0.1.0-alpha.5-windows-x64.zip)

Extract the whole ZIP and run **n3.exe**. Supply an ISO made from your own
supported USA/Europe disc, or your own extracted `default.xex` and `data` folder.
No game files or saves are included. Existing setup and profiles are reused.

### Native engine foundations

- The frame-clock initializer and live rate calculations now use a portable,
  typed C++ module with a separate guest-memory adapter. Unknown clock modes
  retain the original translated implementation.
- Five sphere, capsule and box bounds helpers now use readable native C++.
  Floating-point rounding, signed zeros and negative dimensions keep the
  original behavior. Exceptional inputs, overlapping memory and unmasked
  floating-point exceptions retain the translated code path.
- The disc verifier/importer and menu asset transformations can build without
  the game runtime. Portable components are checked on Windows, Linux and
  Apple Silicon; the **playable release remains Windows x64 with Direct3D 12**.

This release retains the N3 application/shortcut icon, live **VSync On/Off**,
**30/40/60 FPS**, filtering up to **16x**, display sizes through **4K**, and
**720p/1440p/4K internal rendering**. Graphics settings apply without a restart
and save locally. Vulkan is not included.

### Validation and limits

Both Windows build configurations pass **eleven native suites**. Clock tests
compare 18 cases against the retained translated initializer; shape tests compare
**12,800 cases**, including rounding modes, edge values, aliasing, whole-buffer
writes and indirect-call registration. Portable CI passes on Windows, Linux and
Apple Silicon. Packaged file hashes, binary privacy checks and icon resources
are verified, followed by an isolated Direct3D startup check.

This work establishes native interfaces for future modifications. **No performance
gain or corrected animation/combat timing is claimed.** 40/60 FPS remain
experimental; animations can appear accelerated. Full-game coverage, later
missions/characters and other PCs remain unvalidated. The opening mission,
following cutscene, saving and loading were previously exercised on the opening
route; synthetic tests and startup checks do not extend that gameplay coverage.

### Updating

Extract into a fresh application folder and keep `%LOCALAPPDATA%\N3Recompiled`,
which holds saves, settings, imported data and the remembered game path. Recreate
your desktop shortcut if the application folder changed; shortcuts made from
`n3.exe` inherit the embedded N3 icon.
Windows 10/11 x64, Direct3D 12 and an Xbox-compatible controller remain the target.
No Python, Node.js or build tools are required to play.

### Developer sources — optional

[SDK, dependencies and renderer extensions](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/releases/download/v0.1.0-alpha.5/ReXGlue-bd833a2-corresponding-source.zip)
are available for rebuilding and licensing; **they are not needed to play**.
Native game modules are in this project's matching source tag.

[SHA-256 checksums](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/releases/download/v0.1.0-alpha.5/SHA256SUMS.txt)
cover both ZIPs. The Windows package includes third-party notices.
