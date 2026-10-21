# Native engine modules

The translated PowerPC game already executes as native machine code. The modules
below replace understood engine behavior with typed C++ that can be inspected,
tested without the Xbox runtime, and extended through explicit adapters. A
replacement is not a performance claim or proof of broader game compatibility.

## Frame clock

| Layer | File | Responsibility |
| --- | --- | --- |
| Native model | `src/native/frame_clock.h` and `.cpp` | Game timebase frequency, frame deadline, integer ticks per frame and live rate changes |
| Guest adapter | `src/guest_frame_clock.h` | Read/write the verified big-endian clock fields and enable byte |
| Runtime hook | `src/frame_rate.cpp` | Clock source, selected FPS, frame-boundary calls, and guest calling convention |

The hook at `0x82248D38` now initializes recognized 30/40/60 Hz game clocks through
the native model. Unrecognized rates use the retained translated implementation.
Original mode preserves the game's exact float step bits. Live switching changes
the period/deadline while preserving the enable byte, including suspended clocks.
The global guest/audio/movie clock frequency and scalar are unchanged.

The adapter covers this layout for executable version 0.0.0.3:

| Offset | Type | Meaning |
| --- | --- | --- |
| `+1` | byte | Enabled; set to 1 only by initialization |
| `+4` | uint32 | Frames per second |
| `+8` | float32 bits | Simulation step in seconds |
| `+16` | uint64 | Ticks per second |
| `+24` | uint64 | Frame start/deadline origin in game ticks |
| `+40` | uint64 | Integer ticks per frame |

The pinned runtime's frequency import returns 32 bits; the hook deliberately
keeps that truncation. This avoids an accidental semantic change when rewriting
the routine. The native model accepts an explicit frequency and timestamp, so
tests and future engine callers do not depend on a host clock or global state.
All other bytes remain unchanged. Callers keep their stack and nonvolatile
registers; the initializer returns the original success value in `r3`.

## Verification

`native_frame_clock` runs in the portable component build on Windows, Linux and
Apple Silicon. It checks units, integer truncation, exact original step bits,
rate changes, zero-frequency behavior, and rejection without partial mutation.

`frame_rate_policy` compares the replacement with the actual retained translated
initializer for 18 rate/frequency combinations. It compares the entire 48-byte
clock object, including padding and flags, normalizing only the nondeterministic
timestamp; the replacement timestamp must lie between two surrounding samples
of the same game clock. Cases include a frequency wider than the import's return
type. The test also checks calling convention, indirect hook registration, live
switches, suspended clocks, unrecognized rates, and global-clock preservation.

This does not fix accelerated animation at higher FPS. Animation, combat and
scripted-event update paths still require matched-scene tracing and comparisons
before replacing or retiming them. Keep those changes separate from extraction
of an already-understood clock primitive.

## Adding the next module

Identify a bounded engine routine and its inputs, outputs, units, side effects,
ownership and thread. Keep the original translated entry available. Write the
portable model first, then a narrow guest-memory adapter. Compare its results
against the original on supported data, preserve unknown cases, and validate
the affected gameplay route before claiming compatibility or a speedup.
