## [Download for Windows (x64)](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/releases/download/v0.1.0-alpha.3/N3-Recompiled-v0.1.0-alpha.3-windows-x64.zip)

Extract the whole ZIP and run **n3.exe**. Supply an ISO made from your own
supported USA/Europe disc, or your own extracted `default.xex` and `data` folder.
No game files or saves are included. Existing setup and profiles are reused.

### VSync and graphics fixes

- **VSync On/Off** is now selectable in Graphics. It controls Windows
  presentation independently of Xbox vblank timing and applies without a restart.
- VSync uses a nonblocking presentation queue so enabling it keeps menu input
  responsive. The menu-lag regression has been checked interactively.
- Display presets now apply exact 720p, 1080p, 1440p and 4K client dimensions,
  accounting for DPI scaling and restoring maximized/fullscreen windows.
  Oversized normal windows can extend beyond the desktop.
- Menu frame pacing follows the requested FPS before the gameplay clock exists.
- Anisotropic **Off** disables anisotropy consistently. Eligible textures use the
  requested 2x/4x/8x/16x sampler limits; Game default retains the original policy.
- Renderer DLL staging now refreshes correctly after GPU-only rebuilds.

Graphics retains **30/40/60 FPS**, **Game default/Off/2x/4x/8x/16x filtering**,
independent display size and **720p/1440p/4K internal rendering**, local saves,
and the saved-settings confirmation.

### Validation and limits

Release and RelWithDebInfo pass eight native suites. Renderer probes check all
display pixel sizes, all internal source sizes, actual sampler descriptors and
eligibility rules, and synchronized/immediate DXGI presentation policies. Manual
testing confirms the revised VSync path removes the menu lag.

**40/60 FPS remain experimental:** accelerated animation/combat timing is still
under investigation. A frame-rate setting is a target cap; demanding scenes may
run below it. Prerecorded videos retain their own rate. VSync cadence depends on
display refresh; 40 FPS is most evenly paced on a compatible refresh rate such
as 120 Hz. Full-game coverage and other PCs remain unvalidated.

The opening mission, following cutscene, saving and loading have been exercised
on the opening route. These graphics checks do not establish a full playthrough.

### Updating

Extract into a fresh application folder and keep `%LOCALAPPDATA%\N3Recompiled`,
which holds saves, settings, imported data and the remembered game path.
Windows 10/11 x64, Direct3D 12 and an Xbox-compatible controller remain the target.
No Python, Node.js or build tools are required to play.

### Developer sources — optional

[SDK, dependencies and renderer extensions](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/releases/download/v0.1.0-alpha.3/ReXGlue-bd833a2-corresponding-source.zip)
are available for rebuilding and licensing; **they are not needed to play**.

[SHA-256 checksums](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/releases/download/v0.1.0-alpha.3/SHA256SUMS.txt)
cover both ZIPs. The Windows package includes third-party notices.
