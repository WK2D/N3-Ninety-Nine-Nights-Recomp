First **Windows x64 alpha** of N3 Recompiled.

The opening mission has been completed, the following cutscene played, and a save
written successfully. This is early compatibility coverage, with further missions,
other characters, other PCs, and loading saves across restarts still to test.

### Download and run

1. Download `N3-Recompiled-v0.1.0-alpha.1-windows-x64.zip` and extract the whole ZIP.
2. Run `n3.exe` on Windows 10/11 x64 with a Direct3D 12 capable GPU.
3. Select an ISO from **your own USA/Europe disc**, or `default.xex` in your own
   extracted game folder, with its `data` directory beside it.
4. Confirm the one-time ISO import if applicable; about **5.8 GB** of free space
   is required. The source ISO is unchanged and the local game path is remembered.

**No game files, original executable, disc image, or saves are included.** The
matching executable's SHA-256 is checked before use. Supported executable version:
`0.0.0.3`; see `supported-game.json` in the ZIP. Other revisions are rejected.
Node.js, Python, and build tools are not required to play.

The app starts in a **1280×720 window**. An Xbox-compatible controller is
recommended. Saves, settings, imported data, caches, and logs default to
`%LOCALAPPDATA%\N3Recompiled`. Preserve this folder when updating.

### Changes and checks

- Native disc/folder setup, remembered game location, and revision verification.
- Bounded XDVDFS importer with cancellation, failed-import cleanup, and refusal
  to overwrite existing destinations.
- Opening-cutscene indirect-target discovery and audio callback FP correction.
- Three native regression suites and five JavaScript extractor tests pass.
- All 3,342 imported files match the verified extraction by SHA-256.
- Import cancellation, destination protection, and remembered-folder validation
  pass integration checks with matching local game data.

### Limits

This is an alpha, not a full-game compatibility claim. Presentation glitches may
remain. The first-launch picker/progress UI still needs a completed visual
interaction check. A command-line alternative is documented in `README.txt`.

`SHA256SUMS.txt` covers the Windows ZIP and corresponding-source ZIP. The separate
ReXGlue source archive is for SDK/library source availability and rebuilding; it
is **not needed to play**. Third-party licenses and notices are included.
