## [Download for Windows (x64)](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/releases/download/v0.1.0-alpha.12/N3-Recompiled-v0.1.0-alpha.12-windows-x64.zip)

Extract the whole ZIP and run **n3.exe**. Supply an ISO made from your own
supported USA/Europe disc, or your own extracted `default.xex` and `data` folder.
No game files, saves, profiles or developer logs are bundled. Existing local
setup and saves are reused.

### Native kind flags and conditional state queries

Alpha 12 converts **fourteen related game entries** to typed C++: eleven shared
kind-bit queries, an identity-gated query, a category-gated query and a
byte-preserving override. Explicit flag words, masks and identities can be
inspected and edited independently of guest registers. Numeric flag meanings
remain unnamed until caller tracing establishes their gameplay roles.

The adapters preserve skipped memory reads, wrapped indices, physical-memory
mapping, argument homes, scratch writes and register/control results. An identity
match skips later related/table reads; category 1024 skips the kind table; byte
overrides preserve values through 255 and skip unused fields. Unsupported ranges
and aliases use the retained translations.

The project now has **60 typed native entry points**, including **58 with
guest-memory fixtures** and two scalar-only operations. Eleven of this release's
entries share one small lookup operation; entry counts are not subsystem counts
or a percentage of game conversion. See the
[conversion plan](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/blob/v0.1.0-alpha.12/docs/CONVERSION-PLAN.md).

### Validation and limits

Both current Windows builds pass **44 suites**. The new retained comparison
checks **28,672 calls** (26,464 native and 2,208 retained), plus **80 inaccessible
unused-page cases**. Whole memory and every register/control result match without
scratch normalization. Physical objects, owners, records, tables and homes,
unaligned words, offsets above 4 GiB, wrapped indices and aliases are covered.
**Twenty portable component suites pass on Windows, Linux and Apple Silicon**;
the playable game remains **Windows x64 with Direct3D 12**.

The older intermittent direction-basis comparison issue remains open. It did not
recur in this batch's builds; no cause or runtime fix is claimed. The earlier
failure and replay investigation remain in the
[validation record](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/blob/v0.1.0-alpha.12/docs/STATUS.md).

Package hashes, embedded icon, corresponding sources and private-data exclusions
are checked. The extracted importer and fresh-profile Direct3D startup are
verified. These are synthetic/startup checks, with no new gameplay or performance
claim. Later missions/characters, a full playthrough and other PCs remain
unvalidated. 40/60 FPS remain experimental; animations can appear accelerated.
Vulkan remains parked.

### Updating

Extract into a fresh application folder and keep `%LOCALAPPDATA%\N3Recompiled`,
which contains saves, settings, imported data and the remembered game path.
Update desktop shortcuts if the application folder changes; shortcuts made
from `n3.exe` inherit its embedded icon. No Python, Node.js or build tools are
needed to play. Live frame-rate, filtering, resolution and VSync controls remain.

### Developer sources — optional

[SDK, dependencies and renderer extensions](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/releases/download/v0.1.0-alpha.12/ReXGlue-bd833a2-corresponding-source.zip)
are supplied for rebuilding and licensing; **they are not needed to play**.
The project's native modules are in the matching source tag.
[SHA-256 checksums](https://github.com/WK2D/N3-Ninety-Nine-Nights-Recomp/releases/download/v0.1.0-alpha.12/SHA256SUMS.txt)
cover both ZIPs. Third-party notices are included in the Windows package.
