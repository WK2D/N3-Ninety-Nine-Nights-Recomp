## [Download for Windows (x64)](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/releases/download/v0.1.0-alpha.2/N3-Recompiled-v0.1.0-alpha.2-windows-x64.zip)

Extract the whole ZIP and run **n3.exe**. This is the only download needed to play.
Supply an ISO made from your own supported USA/Europe disc, or your own extracted
`default.xex` with its `data` folder beside it. No game files or saves are included.
The executable revision is checked before startup; ISO import needs about 5.8 GB.

### New Graphics menu

Select **Graphics**, below **Tutorial**, on the main menu:

- **Frame rate:** 30, 40, or 60 FPS. Default: 30 FPS.
- **Anisotropic filtering:** Game default, Off, 2x, 4x, 8x, or 16x.
- **Display resolution:** 720p, 1080p, 1440p, or 4K window/output size.
- **Render resolution:** 720p (1x), 1440p (2x), or 4K (3x) internal rendering.

All controls apply **without restarting the game**. Render changes rebuild GPU
resources and shaders, which can cause a brief pause. Display and render sizes
are independent; maximized/fullscreen output follows the desktop. Display scaling
can round window dimensions slightly.

Settings save locally with a short confirmation. Reopening Graphics retains
the selected values. Invalid legacy profiles produced by unescaped Windows paths
are recovered when their format is recognized. Original Options remains available
for audio, camera, vibration, and contrast.

### Updating

Extract into a fresh application folder. Keep `%LOCALAPPDATA%\N3Recompiled`, which
holds saves, settings, imported data and the remembered local game path. Existing
disc setup is reused. Windows 10/11 x64, Direct3D 12, and an Xbox-compatible
controller remain the target. No Python, Node.js, or build tools are needed to play.

### Validation and known limits

The opening mission, following cutscene, saving and loading progress have been
exercised on the opening route. Menu checks confirm live settings, resolution
changes, re-entry, and the saved confirmation. Release and RelWithDebInfo pass
seven native suites. Isolated renderer tests apply live 1x-to-3x and 3x-to-1x changes.

**40/60 FPS remain experimental.** Animations can run faster than intended;
animation/combat timing is still being investigated. Full-game coverage, later
missions/characters, and other PCs remain unvalidated. High resolutions require
more GPU memory and processing power. First-launch picker/progress interaction
coverage remains limited; the README includes command-line alternatives.

### Developer sources — optional

[SDK, supporting library sources, and GPU extension](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/releases/download/v0.1.0-alpha.2/ReXGlue-bd833a2-corresponding-source.zip)
are provided for rebuilding and third-party licensing. They are **not needed to
play**. The package includes notices and permits compatible replacement runtime/GPU DLLs.

[SHA-256 checksums](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/releases/download/v0.1.0-alpha.2/SHA256SUMS.txt)
cover the Windows and developer-source ZIPs.
