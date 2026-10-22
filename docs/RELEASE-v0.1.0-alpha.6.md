## [Download for Windows (x64)](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/releases/download/v0.1.0-alpha.6/N3-Recompiled-v0.1.0-alpha.6-windows-x64.zip)

Extract the whole ZIP and run **n3.exe**. Supply an ISO made from your own
supported USA/Europe disc, or your own extracted `default.xex` and `data` folder.
No game files, saves or profiles are included. Existing local setup is reused.

### More editable native engine code

Alpha 6 adds twelve typed native C++ replacements on top of Alpha 5's six,
bringing the total to **18 engine entry points**:

- Three vector accumulation helpers and four synchronized three/four-component
  setters, with explicit vector inputs and current/cached values.
- A frame countdown with explicit active ID, remaining frames, duration and
  progress, preserving the original decrement and wraparound behavior.
- Scalar addition/subtraction with the original operand rounding and signed-zero
  result flushing, plus a packed-bit reader with explicit bit offset and width.
- Ratio-based vector accumulation with separate numerator and denominator.

The native modules separate understood behavior from guest-memory adapters and
retain the original translated routines for unsupported cases. This makes the
verified operations easier to inspect and modify. Surrounding gameplay roles
still require caller tracing; these are not yet user-facing mod controls.

The release retains the embedded N3 icon and live Graphics controls: **30/40/60
FPS**, filtering through **16x**, display sizes through **4K**, **720p/1440p/4K
internal rendering**, and **VSync On/Off**. Settings save locally and apply without
a restart. **The playable release is Windows x64 with Direct3D 12.**

### Validation and limits

Both Windows build configurations pass **15 test suites**. Tests compare the new
vector/channel entries against the original translations in **14,336 cases**, and
the countdown/scalar/bit/ratio batch in **20,560 cases**. The existing clock and
shape comparisons also pass. Checks cover rounding/flush modes, edge values,
aliases, condition/carry flags, calling convention and indirect registration.
Six portable component suites pass on Windows, Linux and Apple Silicon; this
does not establish a playable Mac or Linux build.

Package file hashes, embedded icon resources, corresponding sources and binary
privacy scans are checked, followed by an isolated Direct3D startup run.
**No performance gain or corrected animation/combat timing is claimed.**
40/60 FPS remain experimental; animations can appear accelerated. Full-game
coverage, later missions/characters and other PCs remain unvalidated. The opening
mission, following cutscene, saving and loading were previously exercised;
synthetic tests and startup checks do not extend that gameplay coverage.

### Updating

Extract into a fresh application folder and keep `%LOCALAPPDATA%\N3Recompiled`,
which holds saves, settings, imported data and the remembered game path. Recreate
your desktop shortcut if the application folder changed; shortcuts made from
`n3.exe` inherit the embedded N3 icon. No Python, Node.js or build tools are needed
to play. Windows 10/11 x64, Direct3D 12 and an Xbox-compatible controller remain
the target.

### Developer sources — optional

[SDK, dependencies and renderer extensions](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/releases/download/v0.1.0-alpha.6/ReXGlue-bd833a2-corresponding-source.zip)
are available for rebuilding and licensing; **they are not needed to play**.
The native engine modules are in this project's matching source tag.

[SHA-256 checksums](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/releases/download/v0.1.0-alpha.6/SHA256SUMS.txt)
cover both ZIPs. The Windows package includes third-party notices.
