## [Download for Windows (x64)](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/releases/download/v0.1.0-alpha.7/N3-Recompiled-v0.1.0-alpha.7-windows-x64.zip)

Extract the whole ZIP and run **n3.exe**. Supply an ISO made from your own
supported USA/Europe disc, or your own extracted `default.xex` and `data` folder.
No game files, saves or profiles are included. Existing local setup is reused.

### Numeric filtering choices

Anisotropic filtering now offers **1x, 2x, 4x, 8x and 16x**. 1x disables
anisotropy. Saved Game default overrides become 1x; existing 2x/4x/8x/16x settings
are retained. The selectors, saved values and renderer overrides use the same
mapping. Filtering applies to eligible mipmapped textures.

### More native C++ engine code

Alpha 7 adds eight typed native replacements since Alpha 6, bringing the total
to **26 engine entry points**:

- Transformed box bounds and corner sampling, capsule world bounds, and box
  enclosing radius with explicit geometry and transform inputs.
- Two fixed-block delay/mixing kernels with editable taps and history state.
- Two feedback-delay kernels with explicit delay, gains, cached output and
  128/256-sample histories. A rejected trial leaves caller state unchanged before
  falling back to the retained translation.

These build on the clock, shape, vector, countdown, scalar and bit-reading
modules. Unsupported cases retain their original translations. These changes
make verified engine behavior easier to inspect and modify; they do not add
user-facing audio effects or gameplay mods.

The release retains the embedded N3 icon and live Graphics controls: **30/40/60
FPS**, filtering through **16x**, display sizes through **4K**, **720p/1440p/4K
internal rendering**, and **VSync On/Off**. Settings save locally and apply without
a restart. **The playable release is Windows x64 with Direct3D 12.**

### Validation and limits

Both Windows configurations pass **23 test suites**. New geometry batches each
compare **16,416 cases** against their retained translations; the delay/mixing
and feedback batches each compare **5,024 block cases**. They cover rounding and
flush modes, history wrap, aliases, exceptional inputs, late-overflow rollback,
registers/flags and indirect registration. All existing regression suites pass.
**Ten portable component suites pass on Windows, Linux and Apple Silicon**;
this does not establish a playable Mac or Linux build.

An isolated Direct3D graphics audit verifies all five actual sampler limits,
display/render presets and both VSync policies. Package hashes, embedded icon,
corresponding sources and binary privacy scans are checked, followed by a
fresh-profile startup run of the packaged application.

**No performance gain or corrected animation/combat timing is claimed.**
40/60 FPS remain experimental; animations can appear accelerated. Full-game
coverage, later missions/characters, listening validation of the converted audio
kernels and other PCs remain unvalidated. The opening mission, following
cutscene, saving and loading were previously exercised. Synthetic and startup
checks do not extend that gameplay coverage. Vulkan remains parked.

### Updating

Extract into a fresh application folder and keep `%LOCALAPPDATA%\N3Recompiled`,
which holds saves, settings, imported data and the remembered game path. Recreate
your desktop shortcut if the application folder changed; shortcuts made from
`n3.exe` inherit the embedded N3 icon. No Python, Node.js or build tools are needed
to play. Windows 10/11 x64, Direct3D 12 and an Xbox-compatible controller remain
the target.

### Developer sources — optional

[SDK, dependencies and renderer extensions](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/releases/download/v0.1.0-alpha.7/ReXGlue-bd833a2-corresponding-source.zip)
are available for rebuilding and licensing; **they are not needed to play**.
The native engine modules are in this project's matching source tag.

[SHA-256 checksums](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/releases/download/v0.1.0-alpha.7/SHA256SUMS.txt)
cover both ZIPs. The Windows package includes third-party notices.
