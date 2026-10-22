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

## Shape bounds

Five additional engine entries now use `src/native/shape_bounds.h` and `.cpp`
through the guest adapter in `src/shape_bounds_hooks.cpp`:

| Entry | Native operation | Input fields | Output |
| --- | --- | --- | --- |
| `0x8235FB80` | Sphere bounds around a center | Center at `+68`, radius at `+288` | Six min/max float components |
| `0x8235FBE8` | Sphere bounds around the origin | Radius at `+288` | Six min/max float components |
| `0x82366970` | Local capsule bounds along Y | Radius at `+288`, half-height at `+292` | Six min/max float components |
| `0x82360478` | Local box bounds | Three half-extents at `+292` | Six min/max float components |
| `0x82366918` | Capsule's enclosing sphere | Center at `+68`, radius at `+288`, half-height at `+292` | Center and combined radius |

These names describe the verified arithmetic and layout; surrounding game class
names and all users of these bounds have not been reconstructed. Units remain
game units. Native operations accept explicit values and own no guest memory.
The hooks read/write big-endian fields and preserve the original return register,
stack, nonvolatile state, rounding mode and scalar flush-mode transition.

Arithmetic keeps the original double-intermediate/float32-rounding sequence.
Floating-point contraction is disabled, and strict/dynamic rounding is enabled
for the native library. The capsule's lower Y extent deliberately remains
`-half_height - radius`, since rewriting it as `-(half_height + radius)` changes
directed-rounding and signed-zero cases. Negative dimensions are not clamped.

NaNs, infinities and subnormal source values retain the translated implementation.
Outputs overlapping the first 304 bytes of the source also use the original,
preserving its ordered reads and writes. The conservative adapter does not try
to broaden these cases for a larger native-coverage count. Unmasked host FP
exceptions also use the original, retaining its partial-write order if a trap
occurs.

## Vector accumulation and synchronized channels

`src/native/vector_ops.h` exposes editable vector operations independent of guest
addresses, PPC registers, or the renderer. `src/vector_ops_hooks.cpp` binds seven
additional game entries to these operations:

| Entry | Verified behavior |
| --- | --- |
| `0x8208ECF8` | Add the vector at `+64`, multiplied by the double scalar in `f1`, to the accumulator at `+564` |
| `0x8209D858` | Same operation using the vector at `+32` |
| `0x8209D8A0` | Same operation using the vector at `+48` |
| `0x82336D90` | Set a three-component value at `+4` and its cached copy at `+60` |
| `0x82336DD0` | Set a four-component value at `+16` and its cached copy at `+72` |
| `0x82336E48` | Set a three-component value at `+32` and its cached copy at `+88` |
| `0x82336EB0` | Set a three-component value at `+44` and its cached copy at `+100` |

`AccumulateScaledVector` separates the float32-rounded product from the addition
and retains the caller's double scalar precision. `SetMirroredVector` updates
both copies; the guest adapter publishes the cache before the current value,
matching the original. `math_types.h` shares the Vec3/Vec4 types with other native
modules. Callers can supply new vectors/scalars or change the native operation
without editing translated code. For example:

```cpp
auto adjusted = n3::native::AccumulateScaledVector(current, direction, amount);
n3::native::SetMirroredVector(channel, adjusted);
```

This is code-level control over the verified arithmetic and storage. Which
objects/channels represent player movement, orientation, animation or other
state still requires caller tracing. No player movement multiplier or gameplay
retiming is inferred from field offsets alone.

The adapters retain the translated path for exceptional/subnormal inputs,
unmasked FP exceptions, wrapping address ranges and overlapping channel inputs.
They share a reviewed FP-control guard with the shape hooks. Current/cached
updates are not a new synchronization primitive and must remain on the game's
existing owner thread.

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

`native_shape_bounds` checks geometric outputs, signed zeros, negative dimensions,
and exceptional-value classification without ReXGlue or game data.
`shape_bounds_equivalence` executes 12,800 comparisons with the five actual
translated entries across four rounding modes and both starting flush modes.
It compares the entire fixture buffer, covering finite and arbitrary float bits,
overflow, subnormals, infinities, NaN payloads, unaligned output and alias fallback.
It also checks guest calling convention, FP controls and indirect registration.
These are synthetic bounds checks, not full collision/mission validation or a
performance measurement.

`native_vector_ops` checks explicit vector edits, signed zeros, scalar precision
and synchronized three/four-component values without ReXGlue. The runtime suite
`vector_ops_equivalence` compares all seven entries with the retained translations
in 14,336 cases, across four rounding modes and both starting flush modes. It
checks whole-buffer writes, unaligned objects/inputs, alias fallback, float and
double edge patterns, indirect-call registration, preserved inputs and nonvolatile
state. The existing shape comparisons also run after sharing the FP guard.

These replacements do not fix accelerated animation at higher FPS. Animation, combat and
scripted-event update paths still require matched-scene tracing and comparisons
before replacing or retiming them. Keep those changes separate from extraction
of an already-understood clock primitive.

## Adding the next module

Identify a bounded engine routine and its inputs, outputs, units, side effects,
ownership and thread. Keep the original translated entry available. Write the
portable model first, then a narrow guest-memory adapter. Compare its results
against the original on supported data, preserve unknown cases, and validate
the affected gameplay route before claiming compatibility or a speedup.
