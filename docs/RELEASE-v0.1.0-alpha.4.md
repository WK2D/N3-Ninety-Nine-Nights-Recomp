## [Download for Windows (x64)](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/releases/download/v0.1.0-alpha.4/N3-Recompiled-v0.1.0-alpha.4-windows-x64.zip)

Extract the whole ZIP and run **n3.exe**. Supply an ISO made from your own
supported USA/Europe disc, or your own extracted `default.xex` and `data` folder.
No game files or saves are included. Existing setup and profiles are reused.

### New application icon

- The emerald-and-silver **N3 icon** is embedded in the Windows executable and
  applied to the application window.
- Windows shortcuts created from `n3.exe` inherit the same icon. No separate
  icon download is needed. Seven sizes cover 16 through 256 pixels.
- To add it to the desktop, right-click `n3.exe`, choose **Show more options**
  on Windows 11, then **Send to → Desktop (create shortcut)**. Keep the
  extracted application folder in its chosen location.

This release retains alpha 3's live **VSync On/Off**, responsive menu input,
**30/40/60 FPS**, filtering up to **16x**, display sizes through **4K**, and
**720p/1440p/4K internal rendering**. Graphics settings apply without a restart,
save locally, and show a saved-settings confirmation.

### Validation and limits

Release and RelWithDebInfo pass eight native suites. The packaged executable's
icon resources load at all seven sizes. Packaged file hashes and binary privacy
checks pass. This release changes branding and documentation; it does not extend
gameplay coverage beyond the opening mission, following cutscene, saving and
loading already exercised on the opening route.

**40/60 FPS remain experimental:** animations can appear accelerated. Frame rate
is a target cap, and demanding scenes may run below it. Prerecorded videos retain
their own rate. Full-game coverage and other PCs remain unvalidated.

### Updating

Extract into a fresh application folder and keep `%LOCALAPPDATA%\N3Recompiled`,
which holds saves, settings, imported data and the remembered game path. Recreate
an existing desktop shortcut if its application folder changed.
Windows 10/11 x64, Direct3D 12 and an Xbox-compatible controller remain the target.
No Python, Node.js or build tools are required to play.

### Developer sources — optional

[SDK, dependencies and renderer extensions](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/releases/download/v0.1.0-alpha.4/ReXGlue-bd833a2-corresponding-source.zip)
are available for rebuilding and licensing; **they are not needed to play**.

[SHA-256 checksums](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/releases/download/v0.1.0-alpha.4/SHA256SUMS.txt)
cover both ZIPs. The Windows package includes third-party notices.
