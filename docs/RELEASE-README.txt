N3 Recompiled - v0.1.0-alpha.7 (Windows x64)

GET STARTED
1. Extract the entire Windows x64 ZIP to a writable folder.
2. Double-click n3.exe. It starts in a 1280 x 720 window.
3. Select an ISO made from your own Ninety-Nine Nights disc, or select
   default.xex in an already extracted copy with its data folder beside it.
4. For an ISO, confirm the one-time import (about 5.8 GB of free space).
   The original ISO is unchanged. Setup remembers the local game folder.

No original executable, game assets, saves, or disc image are provided.
The program reads your selected files locally; there is no upload step.
No Node.js, Python, or development tools are needed to run this package.

DESKTOP ICON
The emerald N3 icon is embedded in n3.exe and used by the application window.
To make a desktop shortcut, right-click n3.exe, choose Show more options on
Windows 11, then Send to > Desktop (create shortcut). The shortcut inherits the
embedded icon. Keep the extracted application folder in its chosen location.

REQUIREMENTS
- Windows 10/11 x64 and a Direct3D 12 capable GPU/driver.
- An Xbox-compatible controller is recommended.
- Supported disc: USA/Europe, title 4D5307DB, executable version 0.0.0.3.
- default.xex SHA-256:
  0fe8973b6ff45d3f8bd03a51a8c0d8cdf278b3d43e793497777e075f4e94a804
Other executable revisions are rejected. Title updates are not validated.

LOCAL FILES
Saves, configuration, remembered game location, imported disc data, caches,
and logs default to %LOCALAPPDATA%\N3Recompiled. Keep this folder when updating.
Replace the application folder with a freshly extracted release to update.
An existing extracted game folder stays in its original location. Keep it
available, or the setup picker will ask you to select matching data again.
To select another game copy, remove only game-path.txt from the profile folder.
Never remove the profile folder to reset setup: it also contains saves.
Controller bindings and other runtime settings are available in the SDK overlay.

GRAPHICS
Select Graphics below Tutorial on the main menu. Up/down selects a row;
left/right changes its value; B returns. Settings save locally with a brief
confirmation and remain selected when reopening the page.
- Frame rate: 30 (default), 40 or 60 FPS.
- Anisotropic filtering: 1x, 2x, 4x, 8x or 16x.
- Display resolution: 720p, 1080p, 1440p or 4K window/output size.
- Render resolution: 720p (1x), 1440p (2x) or 4K (3x) internal rendering.
- VSync: On or Off, applied to Windows presentation independently of game timing.
1x disables anisotropy. Saved Game default overrides now become 1x; existing
2x/4x/8x/16x values are retained. All five controls apply without restarting. Render changes can briefly pause
while GPU resources and shaders are rebuilt. Selecting display size restores a
maximized/fullscreen window and applies exact physical client dimensions. Large
windows may extend beyond the desktop. Display size and internal rendering are
independent. 1080p is available as display output; internal scales are integer.

OPTIONAL COMMAND LINE
n3_import.exe inspect "path\to\your-disc.iso"
n3_import.exe import "path\to\your-disc.iso" "new\game-folder"
n3.exe --game_data_root "path\to\game-folder"
Explicit game-data folders are verified before startup. Import destinations
must not exist; unsuccessful/cancelled imports clean up their newly created tree.

NATIVE ENGINE MODULES
This release includes 26 typed native C++ replacements: the frame-clock
initializer, sphere/capsule/box bounds and projections, vector updates and cached
channel setters, a frame countdown, scalar arithmetic, bit reading, ratio-based
vector accumulation, and fixed-block delay/feedback processing. Native modules
accept explicit state and inputs separately from guest registers and memory
addresses. Tests compare them against retained translated routines, preserving
original rounding, flags and fallback for unsupported cases. Twenty-three
Windows suites and ten portable component suites on Windows, Linux and Apple
Silicon pass. The playable game remains Windows x64 with Direct3D 12. These
changes do not establish a speed improvement or correct accelerated animations
in higher frame-rate modes.

ALPHA STATUS
The first mission, the following cutscene, and writing a save have been exercised
successfully on the opening route; loading saved progress also succeeds there.
Higher frame-rate modes remain experimental: animations can appear accelerated.
A full playthrough, later characters/missions, and other PCs remain unvalidated. Presentation
glitches may remain. Build and importer checks do not establish gameplay coverage.
The first-launch picker/progress UI has not completed an automated visual check.

THIRD-PARTY SOFTWARE
ReXGlue and its dependencies are credited in THIRD_PARTY.md and licenses/.
The Xenos plugin includes the project's live-resolution extension; its source
and build scripts accompany the matching SDK sources.
FFmpeg and libmspack include LGPL components. The matching SDK and dependency
sources and build scripts are supplied as a separate corresponding-source ZIP
on this release. Compatible rebuilt runtime/GPU DLLs may replace the supplied
DLLs; reverse engineering for debugging modifications to LGPL components is
permitted. Source licensing details are preserved with those components.
This software is based in part on the work of the Independent JPEG Group.
Microsoft C/C++ runtime DLLs are included under Microsoft's applicable terms.

PROJECT AND BUG REPORTS
https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp
State the release, character/mission, and reproduction steps when reporting a bug.
Review logs before sharing: they can contain local paths and system details.
Do not upload game files, saves, disc images, or other personal material.
