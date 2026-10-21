# N3 Recompiled

A native Windows recompilation project for **N3: Ninety-Nine Nights**, built from
the Xbox 360 executable with [ReXGlue](https://github.com/rexglue/rexglue-sdk).
PowerPC game code is translated to C++ ahead of time. ReXGlue supplies the Xbox
runtime and its Xenos GPU plugin uses Direct3D 12.

**Current milestone:** the windowed build reaches the main menu, character
selection, and the opening cutscene. Cutscene playback remains unstable; a missing
indirect function entry found at the first crash has been added and rebuilt, with
scene replay validation pending. Gameplay, saves, and full-game completion are
not yet validated. See [development status](docs/STATUS.md).

No game assets, original executable, generated game translations, or downloaded
toolchain binaries are tracked. Supply your own game data. The supported executable
is identified in [game-build.json](docs/game-build.json).

## Run the current build

Double-click **N3 Recompiled** on the desktop, or run:

```powershell
pwsh -NoProfile -File tools/launch.ps1
```

The launcher uses a **1280 x 720 window**, keeps fullscreen off, and stores saves,
profile data, and caches under `private/`. Runtime logs go under `logs/`.
The shortcut follows the stable launcher and build path, so rebuilding updates
what it launches. Refresh the shortcut after moving the project folder:

```powershell
pwsh -NoProfile -File tools/install-shortcut.ps1
```

## Build from a fresh checkout

Windows x64 is the platform tested so far. Setup needs PowerShell 7, Node.js 18+
for disc extraction, Python 3.10+ for portable Microsoft tools, and 7-Zip for
unpacking the LLVM installer. Setup extracts tools locally; it does not install
Visual Studio globally. Download sizes total roughly 1 GB, plus extracted tools
and approximately 5.8 GB of game files.

```powershell
# AcceptVisualStudioLicense accepts Microsoft's development-tool licence.
pwsh -NoProfile -File tools/bootstrap.ps1 -AcceptVisualStudioLicense -Python python

node tools/disc.mjs extract "path/to/your-disc.iso" private/game private/disc-manifest.json
pwsh -NoProfile -File tools/build.ps1
pwsh -NoProfile -File tools/launch.ps1
```

Alternatively, supply an existing VS 2022 x64 development environment and use
`bootstrap.ps1 -ToolsOnly`. Tool versions, download URLs, and archive checksums
are in [toolchain.lock.json](tools/toolchain.lock.json). The Microsoft downloader
pins VS 2022 / MSVC 14.44 / Windows SDK 26100; Microsoft's servicing manifests can
update the patch versions within those selections.

Extraction refuses an existing output directory. Each extracted file receives
a SHA-256 hash in the local disc manifest. Build/codegen verifies the expected
`default.xex` hash before using the build-specific function hints.

## Development commands

```powershell
node --test tools/disc.test.mjs
pwsh -NoProfile -File tools/codegen.ps1 -Regenerate
pwsh -NoProfile -File tools/build.ps1 -Configuration RelWithDebInfo -Jobs 4
pwsh -NoProfile -File tools/smoke.ps1 -Seconds 30
pwsh -NoProfile -File tools/symbolize-fault.ps1
```

The build runs the callback floating-point regression test. Smoke tests stop
their own child process at the deadline and record the result under `logs/`;
survival alone does not establish correct rendering or gameplay. Smoke runs use
the same local profile as the launcher, so use separate profile copies for future
save regression testing.

Analysis hints belong in `n3_manifest.toml`, and runtime fixes belong in `src/`.
Do not hand-edit generated translations. `generated/rexglue.cmake` is SDK build
integration boilerplate and is the only tracked file in `generated/`.

## References

[LostOdysseyRecomp](https://github.com/freefrank/LostOdysseyRecomp) and
[UnleashedRecomp](https://github.com/hedge-dev/UnleashedRecomp) are the reference
projects for development practices and the longer-term port experience.
See [reference notes](docs/REFERENCES.md) and [third-party notices](THIRD_PARTY.md).
