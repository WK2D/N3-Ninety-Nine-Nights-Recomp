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

## Countdown, scalar arithmetic and bit fields

Five more entries use typed native operations through
`src/engine_primitives_hooks.cpp`:

| Entry | Native interface | Verified behavior |
| --- | --- | --- |
| `0x820801B0` | `AdvanceFrameCountdown` | Update active ID at `+208`, remaining frames at `+224`, and progress at `+216`, using total frames at `+220` |
| `0x824A0C50` | `AddScalar` | Add double operands, round to float32, and flush a subnormal result to signed zero |
| `0x824E7AD0` | `SubtractScalar` | Narrow the second operand to float32 before sign inversion/addition, then flush a subnormal result |
| `0x8246D5F8` | `ReadBitWindow` | Extract an LSB-first field from a little-endian four-byte window |
| `0x82324CD0` | `TryAccumulateVectorRatio` | Add a vector from `r5`, scaled by the float32-rounded ratio `f1 / f2`, to the three components at `+344` |

The countdown is a frame counter with an inactive ID of `0xFFFFFFFF`. It
decrements with unsigned wraparound, tests the result as signed, and keeps the
last progress when expiring. Progress deliberately uses the *previous* remaining
frame count divided by total frames, with both signed integers rounded to
float32 before division. Negative and zero durations retain the original
arithmetic. No conversion to elapsed time or higher-FPS animation fix is made.
The surrounding object's gameplay purpose still needs caller tracing.

Scalar results report whether a subnormal was flushed, allowing the adapter to
preserve the original condition flags. Subtraction cannot be simplified to
`float(first - second)`: the early rounding of the second operand is observable.
NaN, infinity and subnormal input registers retain the original implementation.
Native arithmetic requires the caller's rounding mode and disabled scalar
flush; the adapter provides that transition and retains unmasked trap paths.

The bit reader's native interface accepts a zero-based bit offset within the
first byte and a width of 1 through 32 that fits the four-byte window. The guest
entry's position is one-based. Unsupported widths, crossing windows and wrapping
addresses use the retained PPC shift behavior. This helper reads four bytes;
it is not a bounded parser for arbitrary untrusted buffers.

Ratio accumulation retains float32 ratio/product/result rounding and the
original reversed addition operands for Z. Unsupported inputs, zero denominators,
exceptional ratios and overlapping input/object ranges retain the translation.
The adapter restores FP controls before fallback and writes no partial native
result. Together these interfaces expose explicit frame state, scalar operands,
bit fields and vector inputs for future native callers without guest registers.
They are not yet a gameplay scripting or mod configuration interface.

## Transformed box geometry

`src/native/box_geometry.h` exposes an `AffineTransform3` (three row-major basis
rows plus translation) and `TransformedBox` with explicit half-extents. Two more
entries use this model through `src/box_geometry_hooks.cpp`:

| Entry | Native interface | Verified guest inputs/output |
| --- | --- | --- |
| `0x823602C0` | `TransformedBoxBounds` | Basis at `+32`, translation at `+68`, half-extents at `+292`; six min/max components to `r4` |
| `0x823604B8` | `TryTransformBoxSample` | Same box plus signed integer coordinates in `r4/r5/r6`; three transformed components to `r7` |

Bounds project each half-extent onto a basis row, take the absolute value of each
float32-rounded product, and add Z, then Y, then X. The returned bounds are centered
on the explicit translation. Negative extents retain the original arithmetic;
the transform is not assumed orthonormal, so scale and shear are supported.

The sample routine accepts all signed 32-bit coordinates, including the usual
eight `-1/+1` corner choices. Each integer rounds to float32 before multiplication
by the half-extent. For each output component, the Z product rounds first; the Y
and X terms use explicit double FMAs narrowed to float32, followed by a rounded
translation addition. Substituting float FMA or reassociating the dot product can
change bits. For example, native callers can edit `box.half_extents` or
`box.transform` and recompute bounds/corners without guest registers or addresses.
This is a geometry interface, not an identified player hitbox editor.

The adapters retain the original for exceptional/subnormal source values,
unmasked FP exceptions, wrapping ranges, input/output aliases and overlaps with
the original caller-stack scratch. Unsupported scaled sample coordinates also
fall back without publishing a partial result; saved FP controls are restored
first. Native arithmetic requires caller rounding and disabled scalar flush.
Box state and output remain owned by the game's existing calling thread.

## Oriented capsule bounds and box sphere radius

`src/native/shape_projection.h` adds an `OrientedCapsule` with editable center,
axis, radius and half-height, plus `CapsuleProjection` containing each endpoint's
bounds and their merged bounds. `BoxSphereRadius` accepts explicit box half-extents.
`src/shape_projection_hooks.cpp` binds two additional engine entries:

| Entry | Native interface | Verified guest inputs/output |
| --- | --- | --- |
| `0x82366808` | `ProjectCapsuleBounds` | Axis at `+36/+48/+60`, center at `+68`, radius at `+288`, half-height at `+292`; six min/max components to `r4` |
| `0x823603B0` | `BoxSphereRadius` | Center at `+68`, half-extents at `+292`; center and stored radius to `r4`, double square-root result in `f1` |

Capsule projection rounds axis times half-height to float32, then forms rounded
negative/positive endpoints and radius-expanded bounds. Merging updates only on
strict comparisons, retaining the negative endpoint's bits on ties, including
opposite signed zeros. The axis is not normalized and dimensions are not clamped;
non-unit axes and negative radii/heights retain the original arithmetic. Endpoint
bounds are exposed for native callers and geometry inspection. The adapter also
preserves the original `f1` value and final `cr6` comparison.

The box radius uses a float32-rounded Y square, then explicit double X and Z FMAs
each narrowed to float32, followed by a double square root. Its stored radius
rounds once more to float32, while `f1` retains the double result. A generic
`hypot` or rescaled length would change the original overflow/underflow behavior:
large finite extents can produce an infinite radius, and tiny normal extents can
produce zero. This extraction preserves those cases rather than claiming more
conservative bounds.

Both adapters retain the original for exceptional/subnormal sources, unmasked
traps, wrapping ranges and output/source aliases. Box-sphere source/output overlap
with the original caller stack frame also falls back. Native arithmetic requires
caller rounding, disabled scalar flush and masked traps. State remains on the
game's existing owner thread. These interfaces do not identify player hitboxes or
introduce user-facing geometry edits.

## Fixed-block delay and mixing

`src/native/delay_block.h` exposes `DelayBlockState`: three tap delays/gains, two
previous outputs and a write cursor. `TryProcessDelayBlock` accepts explicit
history, 256 input samples and two 257-sample output buffers. Two entries use
this model through `src/delay_block_hooks.cpp`:

| Entry | History length | Verified state layout |
| --- | --- | --- |
| `0x8253BED8` | 512 float samples | Tap delay/gain at `+0/+4`, `+12/+16`, `+24/+28`; previous outputs at `+8/+32`, cursor at `+36`, history at `+44` |
| `0x8253C240` | 2048 float samples | Same fields, with the larger history mask |

Both kernels read all three history taps before writing the current input. The
mixed output is a double FMA of the first tap/gain and the float32-rounded second
tap/gain product, narrowed to float32. The auxiliary output is the rounded third
tap/gain product. Delays wrap modulo history length; delay zero reads old history
at the write cursor, not the current input. Each output begins with the cached
previous result, followed by 256 new results. The last results become the new
cached values and the cursor advances by 256 samples with wraparound.

Native callers can edit tap delays/gains and process successive blocks without
guest registers or addresses. The verified callers use these fixed-size blocks;
the surrounding effect identity, perceptual behavior and routing still need
tracing. This does not introduce an audio-effect menu or change output gains.

The supported native cursor is block-aligned and leaves a complete contiguous
block within history. The original writes history contiguously, so unsupported
cursors retain that behavior through fallback rather than being silently wrapped
sample by sample. Overlapping state/history/input/output buffers, exceptional or
subnormal source values and unmasked FP traps also retain the original. Rejected
native calls mutate no state or buffers. Finite sources can still produce
overflow/underflow results, preserving the original arithmetic.

The guest adapter preserves untouched fields, output-pointer advancement, cached
results in `f1/f2`, cursor in `r7`, final condition flags and nonvolatile state.
It rejects wrapping guest ranges and overlap with the original 48-byte stack
scratch. A private per-thread workspace avoids large history snapshots on small
callback stacks; processing performs no callbacks and publishes no partial native
result. Existing owner-thread and valid-storage requirements remain in force.
Native arithmetic uses caller rounding, disabled scalar flush and masked traps.

### Two independent taps

Entry `0x8253C758` uses `DualTapDelayState` and `TryProcessDualTapDelay` in the same
module. Its two delays/gains produce separate outputs without mixing them. The
guest layout is delay/gain/previous at `+0/+4/+8` and `+12/+16/+20`, cursor at
`+24`, and 1024 float history samples at `+32`. Each call reads 256 inputs and
writes two 257-sample outputs prefixed by their previous values. Both taps read
old history before the current input is stored, including delay zero.

Supported cursors are 0, 256, 512 and 768; other cursors retain the original.
Delays wrap modulo 1024, while the history write is one contiguous block. Native
callers can edit the gains and delays independently. Source classification,
disjoint-storage checks and FP requirements match the three-tap model. Finite
source products may overflow or underflow just as in the translation; they do
not feed subsequent arithmetic within this kernel.

The adapter checks wrapping guest ranges and the original 32-byte caller scratch,
preserves `f1/f2`, returns both final products in `f0/f13`, and advances all three
buffer pointers by 1024 bytes. `r7` holds the unmasked cursor plus 256, while `r9`
and the stored cursor contain the modulo-1024 result. These distinct results are
compared against the retained translation. Effect roles and listening validation
remain unestablished; this adds an editable engine interface, not an audio menu.

## Feedback-delay processing

`src/native/feedback_delay.h` adds `FeedbackDelayState` with an explicit delay,
output gain, feedback gain, previous output and cursor. `TryProcessFeedbackDelay`
implements entries `0x8253BFA8` (128 history samples), `0x8253C310` (256), and
`0x8253C480` (512) through `src/feedback_delay_hooks.cpp`. The verified guest fields are
delay at `+0`, output/feedback gains at `+4/+8`, previous
output at `+12`, cursor at `+16` and history starting at `+24`. `r4` points to a
257-float in-place buffer; its first 256 values are input samples.

For each sample the model reads the delayed history, computes a float32-rounded
double FMA of feedback gain, delayed value and input, and stores that result back
into history. A second rounded double FMA of output gain, feedback result and
delayed value produces the new output. The output buffer begins with the old
cached output, followed by 256 new results; the final result becomes the next
cache. Delays wrap modulo the history size, including delay zero's old-history
read. The 128-sample entry makes two history passes per block: the second half
reads the updated first-half history. The 512-sample entry writes one 256-sample
half per call, alternating cursors 0 and 256. It preserves the other half and
advances the cursor modulo 512. Native callers can edit both gains and delay
without registers or guest addresses.

The supported cursor is zero for the 128/256 variants and either 0 or 256 for
the 512 variant. The originals write contiguous blocks without per-sample write
wrapping; other cursors retain the original path. Source subnormals,
NaNs/infinities, overlapping storage, wrapping guest ranges, original stack
overlaps and unmasked traps also fall back. The source buffer's output-only last
element is not interpreted as an input value. Generated subnormals are preserved.

Feedback can become nonfinite after several samples. The native model stages
history/output and commits only after the entire block has finite trial results;
rejection changes no caller state or buffers. The adapter restores saved FP
controls/status before invoking the retained translation. Directed-rounding
overflow that remains finite is supported and retains its saturated value.
Two bounded local staging arrays use at most about 3 KiB; the adapter's guest snapshots
use private per-thread storage. Processing invokes no callbacks and remains on
the existing owner thread. Caller rounding, disabled scalar flush and masked
traps are required by the native model. Effect routing and listening validation
are still pending; no gain or timing option is added to the game.

The shorter kernel leaves original input samples 249 and 248 in `f1` and `f2`
and sets carry from the unsigned delay comparison with 2. Its adapter reproduces
these results as well as the common 1,024-byte input-pointer advance and final
condition flags. The 256/512 entries preserve their original `f1/f2` and carry
behavior. All three return the resulting cursor in `r5` and store it in state.
Original translations remain available for fallback.

### Separate output and final sample

Entry `0x8253C5F0` shares the 512-sample history and arithmetic through
`TryProcessFeedbackDelaySeparate`. Its guest buffers have an unusual verified
contract: `r4` points to 256 unchanged input samples plus a writable final slot,
while `r5` points to a separate 256-sample output. That output receives the old
cached prefix and the first 255 new results. The last new result goes into
`input[256]` and becomes the cached output. The adapter preserves this layout
instead of extending the separate output buffer. Existing callers pass distinct
stack buffers and feed the separate output into subsequent processing.

The portable API represents that contract as const input, output and a separate
final-output reference; the reference may immediately follow input. State,
history, input, output and final-output storage must be pairwise disjoint. The
calculation stages its history and results, so late rejection changes none of
those regions. The adapter includes the input tail in range/alias checks, checks
the original 40-byte caller scratch, advances `r4` by 1024 and returns the wrapped
cursor in `r5`. `f1/f2`, carry and nonvolatile state are preserved. Native snapshots
use per-thread storage, and the shared core requires the existing FP contract.

## Table-derived limits and bounded updates

`src/native/bounded_value.h` separates channel selection, limit calculation,
value adjustment and modifier replacement from guest tables. Seven entries use
it through `src/bounded_value_hooks.cpp`; an eighth related-table entry uses
`src/state_relation_hooks.cpp`. The surrounding class, channel meanings
and gameplay units remain unidentified; these names describe the verified work.

| Entry | Operation | Table descriptor offset | Percentage field |
| --- | --- | --- | --- |
| `0x8209DBF0` | Limit with unsigned additive offset | `+88` | `+728` |
| `0x8209D718` | Add signed delta, then lower/upper comparisons | `+88` | `+728` |
| `0x8209DCF0` | Parallel table value, no additive offset | `+8` | `+736` |
| `0x8209DDE8` | Parallel table value, no additive offset | `+40` | `+740` |
| `0x820A13B0` | Replace modifier, then upper-only comparison | `+88` | `+728` |
| `0x8209D688` | Add integer percentage of limit to bounded value | `+88` | `+728` |
| `0x820B4DC8` | Conditional percentage addition to parallel value | `+8` | `+736` |
| `0x8209DEE0` | Table value with related-object category selection | `+216` | `+784` |

Descriptors are begin/end pointer pairs under the shared root plus category
times 16. Parallel getters return zero-extended truncated bits directly.

The limit entry chooses a channel from `+676`. For kind 1000 at `+300`, it first
adds `+780` with 32-bit wrap and caps signed results at least 9 to channel 8.
Negative wrapped results are not clamped upward. The adapter selects a signed
word from a 48-byte table record using row `+304` and category byte `+969`.
It supports whole, forward table ranges and channels 0 through 11; unknown
geometry, indices and channels retain the original path, including its traps.

`ValueLimitParameters` exposes the selected signed base, signed percentage
adjustment (`+728`), unsigned additive offset (`+732`), percentage unit and
baseline. Original constants are 0.01f and 1. Integer sources round to float32;
the percentage/baseline stage uses a float32-rounded double FMA, followed by a
float32-rounded product with the base. The product truncates toward zero with
the original positive saturation. Both the truncated bits and additive offset
are zero-extended before a 64-bit addition. Negative products below INT_MIN,
exceptional/subnormal source constants and nonfinite trials retain the original
conversion path. Rejected trials leave native outputs unchanged.

The adjustment entry updates float field `+664` by a signed integer delta from
`r4`, rounded to float32 before addition and rounded again afterward. It applies
the original lower comparison (zero in the game) before the signed low-32-bit
limit rounded to float32. A negative upper limit can therefore replace a
lower-clamped result. This deliberately preserves the two comparisons rather
than assuming ordered bounds. `BoundedValueUpdate` exposes intermediate and
clamp results for native callers; source exceptions and nonfinite trials reject
without changing state or update details.

`ValueLimitState` exposes limit parameters and the current value together.
`TrySetValueLimitModifier` replaces the signed percentage when mode's low byte
is nonzero, or the unsigned additive offset otherwise. It recomputes the limit
and compares the unchanged current value with the rounded signed low word of
the result. Only an upper comparison occurs: negative values and negative zero
survive when below or equal to the limit. Rejected trials leave both state and
details unchanged. The adapter rejects modifier writes that overlap lookup
pointers, descriptors, selected table cells or constants before following those
sources, preserving the original write-before-read behavior through fallback.

The adapters preserve the full 64-bit additive limit return in `r3`, final
condition/carry behavior, `f0/f1/f2/f3/f4`, nonvolatile registers and LR semantics.
Getter record/index/base registers and the modifier's final `f13` comparison
source are retained. All read regions are checked against original caller
scratch before traversal: 16 bytes for getters and a conservative 128 bytes for
the wrappers, including their nested callee. A wrapper prologue can replace a
root/descriptor pointer, so following its old bits before rejecting is unsafe.
The adapters restore FP controls
and status before unsupported-case fallback. The models require caller rounding,
disabled scalar flush and masked traps. No gameplay retiming or new stat menu is
introduced; concrete class roles still need matched-scene tracing.

`TryComputePercentageDelta` explicitly rounds both signed integer sources to
float32, then separately rounds percentage times unit and fraction times base.
`TryAdjustValueByPercentage` computes the first limit, derives a signed integer
delta from its low word, recalculates the limit, then applies the existing
lower/upper update. Its adapter excludes 256 bytes of nested scratch; no persistent
state changes occur between the limit reads on this supported path.
`TryAddConditionalPercentage` uses either nonzero flag byte to enable the extra
percentage, returning the unchanged base otherwise. Its adapter still computes
the parallel base first, reads the flags in original short-circuit order and
excludes 128 bytes of scratch plus the larger 1380-byte object range.
Zero-extended base and increment bits add in 64 bits, preserving carry.

Unsupported outer percentage calls retain their complete nested translations
through a scoped thread-local guard. A native leaf getter omits scratch writes;
mixing it with a retained outer caller could change later aliased flags or
percentage fields. The guard keeps those writes and original read order together,
and is released when the retained chain returns. The independent oracle switch
exists only in the percentage differential test. Both entries preserve `f1`–`f4`,
return/carry/condition state, the tested incidental registers and LR semantics.

Entry `0x8209DEE0` checks the packed handle from `+340/+344` before selecting its
table category. A matching key whose pointed object has kind 6 or 1000 selects
category 2; otherwise category byte `+969` remains in use. Its own channel uses
the existing kind/channel rule, and the row remains `+304`. The begin/end pair is
at root plus category times 16 plus 216, and signed percentage `+784` replaces
the earlier percentage fields. The same rounded factor/product and truncation
produce a zero-extended 32-bit result without an additive offset.
`SelectRelatedTableCategory` exposes that decision independently of guest memory.
The adapter excludes 240 bytes of nested scratch before following lookup sources,
retains final record/index/base registers, `f0/f13`, `f1`–`f4`, nonvolatile state,
LR and condition/carry behavior, and restores FP controls before rejection.

## Handle records and state payload installation

Four connected entries expose verified record operations without assigning a
gameplay role to the surrounding objects:

| Entry | Native interface | Guest adapter |
| --- | --- | --- |
| `0x82081138` | `TryLookupHandleRecord` | `src/handle_table_hooks.cpp` |
| `0x82080780` | Packed-handle key/index extraction and key validation | `src/handle_table_hooks.cpp` |
| `0x8209D358` | `StatePayload` and `TryCopyStatePayload` | `src/state_payload_hooks.cpp` |
| `0x8209D280` | `TryInstallStatePayload` | `src/state_relation_hooks.cpp` |

The handle table contains 12-byte records with key, object and auxiliary words.
The native API accepts a bounded span. The guest adapter checks a nonempty,
forward, whole-record begin/end pair and the selected index before reading it.
A packed handle's first word supplies the key; its low 16 bits are sign-extended
before the original unsigned bounds check. Key validation compares the full
first word and does not dereference the record's object word.
The validating wrapper's original writes include a 112-byte frame below SP and
16 bytes of argument homes at entry SP plus 16; sources must avoid both ranges.
Unsupported geometry, wrapping, negative/out-of-range indices and source/output
aliases retain the originals, including their invalid-input paths. Scoped guards
retain the complete nested lookup when an outer adapter rejects.

`StatePayload` names groups by offsets in the verified 100-byte record. Integer
words retain all bits; its eight float fields support normal values and signed
zeros. Native copies preserve those values without arithmetic. Exceptional or
subnormal transfers, unmasked FP traps and overlapping guest records retain the
original sequential loads and stores. Guest fields remain big-endian; native
callers edit typed state without guest addresses or registers.

The installer first copies this record to object `+336`. A nonnull optional
vector enables word `+412` and replaces floats `+420/+424/+428`. For signed own
kind at least 6, a matching handle whose related object has kind 1000 and variant
other than `0xFFFFFFFF` multiplies words `+356` and `+360`, storing the low word
back to `+356`. `PayloadInstallUpdate` exposes the full signed 64-bit product.
Source fields, optional vector and relation data are explicit native inputs;
rejection publishes neither state nor result details.

The installer preserves the original branch-dependent return registers, including
the payload pointer on its early branch, handle success or related variant on
later branches, and the full product in `r9` when multiplication occurs. Payload
float registers and optional-vector overrides retain their original values;
`f1`–`f4`, nonvolatile registers, LR and condition/carry behavior remain intact.
Both outer adapters exclude all 240 bytes of nested stack writes, including the
inner validator's argument homes. A payload write that could replace a descriptor,
record or later related-object read retains the original chain. Optional-vector
and copy aliases also retain original read/write order through fallback.

These interfaces prepare connected callers for further conversion. Broader
progression and callback-driven state updates still need explicit ownership,
callback and side-effect contracts; no gameplay role, speedup or new user-facing
editing feature follows from the field offsets alone.

## Direction normalization and conditional accumulation

Two entries now share typed squared-length math without guest addresses or
registers in the native API:

| Entry | Native operation | Guest adapter |
| --- | --- | --- |
| `0x823B3C00` | `TryNormalizeDirection` | `src/direction_normalization_hooks.cpp` |
| `0x8209D8E8` | `TryAccumulateDirection` | `src/direction_accumulation_hooks.cpp` |

`src/native/direction_math.h` exposes component squares and the exact reduction
tree used by the Windows translation: `(+0 + z*z) + (y*y + x*x)`, with each
product and addition rounded separately to float32. Subnormal vector inputs
become signed zero. Positive squared-length overflow is supported; mathematically
tiny nonzero stages and rounded subnormal stages retain the original translation.
Accepted results are independent of the host's input/output flush settings.

Normalization compares the squared length with the supplied threshold. A greater
or equal threshold produces four positive zeros without inspecting the raw
fourth component. Otherwise it uses a separately rounded square root, reciprocal,
Newton correction products, subtraction, sign-bit negation and final multiply.
The fourth component does not contribute to length but is multiplied on the
active path. `DirectionNormalization` exposes every stage. Finite normal/zero
active stages are supported; unsupported tiny, nonfinite or overflowing stages
reject without publishing results. The default guest threshold is `FLT_MIN`.

The adapter reconstructs the original aligned vector loads and byte permutation,
including the fourth lane's wraparound for unaligned inputs. It retains rounded-down
scalar output addresses, all vector-register results, the scalar threshold result,
condition fields and final enabled vector flush mode. It reproduces the 16-byte
scratch and output-pointer home at entry SP plus 20, excluding dependencies on
those writes before taking snapshots. Safe source/output overlaps remain native.
Strict FP environment access keeps threshold conversions on the original side
of the flush transitions. Both host and guest traps must be masked.

`src/native/direction_accumulation.h` exposes current state, default/override
directions, an explicit double amount, lower bound and squared override length.
The override is chosen when its length comparison is greater or unordered;
otherwise the default is used. Each selected component multiplies the amount
and rounds to float32 before a separate addition to current state. Branch-specific
addition operand order is retained. The unselected direction is not inspected.
Finite generated scalar subnormals remain valid with scalar flush disabled.
Unsupported inputs or nonfinite trial results change neither state nor details.

Its guest adapter reads default direction `+64`, current vector `+564` and override
direction `+576`, and writes only current XYZ plus the original 16-byte dot scratch.
It preserves the actual vector-phase rounding when host and guest controls
diverge, then unconditionally restores the guest scalar controls with flush off.
Final scalar/vector registers, pointer upper halves and condition fields match
the original branch. Unsafe scratch aliases, unmasked traps and unsupported
math restore both saved FP words before the retained call. Neither native path
claims identical transient sticky FP exception status.

Normalization has 240 static direct call sites across 147 translated callers.
The accumulation entry has indirect address references but no generated direct
calls; its runtime use has not been observed. The direction-basis caller
`0x8209D9B0`, which invokes normalization three times, now uses the typed phases
described below.
These contracts do not identify an animation/combat role or change game timing.

## Direction basis from signed axes

Entry `0x8209D9B0` now uses `src/native/direction_basis.h` and `.cpp`, with the
guest boundary in `src/direction_basis_hooks.cpp`. Its signed integer pair rounds
to float32 and forms `{horizontal, lower, vertical}`. The vector normalizes and
its new squared length is compared with lower. The inactive branch leaves output
untouched, including arbitrary old bits. A null selected basis writes
`{lower, lower, unit}`. Otherwise the selected record's X/Z values form a second
normalized vector with Y replaced by lower. The native model forms `up cross
basis`, weights that right vector by normalized horizontal and the basis by
normalized vertical, adds the rounded products, and normalizes the result.

Cross products, subtraction and sign-bit negation round separately. Scalar
products round before additions, preserving the different Y operand order and
generated subnormals. The model exposes each stage. Input preparation,
selected-basis composition and final normalization have separate transactional
interfaces, with a convenience operation for ordinary native callers. Guest
pointer reads are deferred until the input branch is active. Integer conversions
can observe incoming host rounding before subsequent vector stages install
guest rounding; the adapter preserves both phases.

Guest selection uses the full original slot arithmetic with a wrapped 32-bit
`slot * 12` address offset. No speculative end bound is invented. Selected fields
are at record `+64/+68/+72`; the middle value is overwritten before normalization.
The adapter writes object `+576/+580/+584`. It reproduces the complete original
stack writes and argument homes, branch-specific returns, all scalar/vector
register results and final disabled scalar flush mode. Raw fourth lanes at frame
`+140` and `+124` remain explicit inputs. Before final normalization, a local byte
overlay reconstructs the raw vector load after staged unaligned scalar stores;
the normalized XYZ stores still round their addresses down to four bytes.

Aliases that could change later constants, selected pointers, saved registers or
stack sources retain the ordered translation. A final output overlapping the
normalization threshold also retains the original post-write threshold read.
Unknown math domains restore both saved FP words before fallback. The native
interface does not identify the selected record's gameplay ownership or timing.

All three direction adapters use `src/guest_memory_address.h`. This matches the
pinned translation's physical-memory host bias of `+0x1000` at guest addresses
`0xE0000000` and above on Windows and Apple ARM64. Address arithmetic and alias
checks use translated host offsets. Ranges crossing that bias boundary retain
the original separate-access sequence. The helper validates arithmetic rather
than establishing that arbitrary guest memory is mapped.

## Capped accumulation and optional accounting

`src/native/capped_accumulator.h` exposes parameters, current integer value,
optional total/count state and calculation details for entry `0x8209D7A8`.
Signed delta and percentage adjustment round separately to float32. A rounded
double FMA creates the factor from percentage, unit and baseline; the product
rounds again before truncation with positive saturation. The resulting bits add
to the old value with 32-bit wrap before a signed upper-cap comparison. The
guest adapter uses cap 999999. It deliberately applies no lower clamp.

The adapter reads percentage `+760`, changes value `+680` and examines owner
pointer `+824`. A nonzero byte at owner `+696` enables global total/count updates
at offsets `+4712` and `+4708` under pointer `0x82B60080`. Both counter sources
are read before either counter write. Low words publish to state while the
64-bit zero-extended sums remain in the original return registers. Disabled
accounting preserves incoming return registers as the original does.

The adapter excludes original 16-byte scratch and rejects a value write that
would alter subsequent owner/global/counter reads before following those sources.
Counter writes to already-consumed sources retain the original order and remain
supported. Trials publish nothing on rejection and restore FP state before
fallback. All original GPR/FPR outputs, LR, condition/carry and control behavior
are checked. Surrounding gameplay roles and units remain unassigned.

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

`native_engine_primitives` checks countdown transitions, wraparound, signed
progress, zero durations, scalar operand narrowing, signed-zero result flushing,
bit order/width validation and explicit ratio vector edits without the runtime.
`engine_primitives_equivalence` compares the five actual translated entries in
20,560 cases across all four rounding modes and both starting flush modes.
It covers double/float edge patterns, all bit offsets and widths through 63,
counter boundaries, alias fallback, unaligned fields, condition/carry flags,
return values, representative nonvolatile state and indirect registration.
Exact arithmetic cases also exercise unmasked exceptions without triggering a
trap. All fixture bytes must match except the caller's 16-byte volatile stack
scratch used only by the original scalar/countdown arithmetic; unmasked fallback
cases compare that scratch too. FP rounding, flush and exception controls match.
These synthetic tests do not establish every caller's gameplay behavior.

`native_box_geometry` checks identity corners, translation, scale/shear, negative
extents, sum order, coordinate rounding and rejection without output mutation.
`box_geometry_equivalence` compares the two actual translations in 16,416 cases
across four rounding modes and both starting flush modes. It checks arbitrary
and edge float patterns, large signed integer coordinates, unaligned fields,
object/output and stack aliases, unmasked exact arithmetic, all nonvolatile
GPR/FPRs, condition/carry flags, FP controls and indirect-call registration.
Ordinary cases explicitly assert the native path is used. Whole-buffer checks
normalize only dead caller stack scratch (24 bytes for bounds, 64 for samples)
in non-overlapping masked cases; alias and unmasked cases compare every byte.
These memory-output routines do not reproduce dead volatile FP/GPR temporaries.
No new gameplay coverage, hitbox-role identification or speedup is established.

`native_shape_projection` checks editable capsule geometry, endpoint merging,
negative dimensions, signed-zero ties, source classification, radius precision
and original square overflow/underflow. `shape_projection_equivalence` compares
the two actual translations in 16,416 cases across all four rounding modes and
both starting flush modes. Checks include arbitrary/edge float bits, explicit
signed-zero ties, unaligned fields, source/output and stack aliases, unmasked
exact arithmetic, `f1` bits, all nonvolatile GPR/FPRs, condition/carry flags, FP
controls and indirect registration. Ordinary fixtures assert native-path use.
Capsule checks compare every fixture byte; box-sphere checks normalize only its
dead 96-byte caller stack frame in non-overlapping masked cases. No further
mission coverage or performance improvement follows from these synthetic checks.

`native_delay_block` checks tap edits, modulo delays, old-history reads, output
prefix/continuity, cursor wrap, unchanged history outside the block and rejection
without mutation. `delay_block_equivalence` compares both translations in 5,024
block cases across all rounding/starting flush modes. Eight-block sequences pass
through wraparound for both history lengths. Whole-buffer checks cover state,
history, input and both outputs, including arbitrary tap delays, unaligned
buffers, finite extremes, exceptional values, unsupported cursors and aliases.
Native cases explicitly prove hook use and normalize only untouched 48-byte
caller scratch; fallback cases compare every byte. Checks include `f1/f2`, pointer
advancement, cursor, all nonvolatile GPR/FPRs, condition/carry flags, exception
controls and indirect registration. These are kernel comparisons, not listening
tests or a performance measurement.

The same suites also cover the independent two-tap entry: portable fixtures check
its distinct gains, output prefixes, four-block cursor wrap, untouched history,
output-only tails and rejection without mutation. `delay_block_equivalence` now
compares all three entries in 7,536 block cases. Its two-tap cases verify the
32-byte scratch boundary, preserved `f1/f2`, final product registers, both cursor
forms, aliases, source exceptions, unmasked exact arithmetic and registration.

`native_feedback_delay` checks gain/delay edits, eight-block feedback continuity,
modulo delays, old-history reads, impulse decay through generated subnormals,
the unread output tail and rejection without partial publication, including late
overflow after the shorter history has wrapped. `feedback_delay_equivalence`
compares all four retained entries in 10,048 block
cases across all four rounding modes and both starting flush modes. It covers
eight-block chains, alternating 512-sample history halves and cursor wrap,
positive/negative overflow and finite saturation, generated
subnormals, source exceptions, unsupported cursors, unaligned/overlapping buffers,
stack aliases and unmasked exact arithmetic. Whole-buffer checks normalize only
unchanged native caller scratch (32 bytes in-place, 40 separate-output); fallback
cases compare every byte.
They also check native-path use, pointer/cursor results, each entry's `f1/f2`, all
nonvolatile GPR/FPRs, condition/carry flags, FP controls and indirect registration.
These are kernel comparisons, not perceptual or performance measurements.

The separate-output cases additionally compare the entire split output and input
tail, explicitly preserve the first 256 input values, and cover aliases involving
either buffer, state, history and the original 40-byte scratch. Portable fixtures
include output canaries, gain edits, split-output continuity, both history halves,
generated subnormals and rollback after a late failure. They also reject aliasing
the final-output reference with state, history, input or output. FP comparisons
cover rounding, flush and exception controls; they do not require every transient
volatile register or every sticky exception-status bit to match.

These replacements do not fix accelerated animation at higher FPS. Animation, combat and
scripted-event update paths still require matched-scene tracing and comparisons
before replacing or retiming them. Keep those changes separate from extraction
of an already-understood clock primitive.

`native_bounded_value` checks channel wrapping/capping, percentage and baseline
edits, modifier mode selection, truncation and positive saturation, 64-bit offset
carry, upper-only versus lower/upper comparisons, signed zeros, rejection without
mutation and integer rounding in all four modes.
`bounded_value_equivalence` compares five retained entries in 10,640 cases using
sparse synthetic guest memory for the original table and constant addresses.
It covers table rows/categories, channels outside the native subset, signed
integer extremes, exceptional float sources, aliases with state or stack,
unaligned fields, original constants and altered values, plus unmasked exact
arithmetic. Whole-fixture bytes match after normalizing only untouched native
scratch; fallback cases compare those bytes too. Ordinary fixtures assert native
path use; 7,483 cases use it overall. Checks include return/condition/carry state,
`f0/f1/f2/f3/f4`, getter record/base registers, modifier `f13`, all
nonvolatile GPR/FPRs, indirect registration and FP controls. A switch compiled
only into the test target forces the retained nested limit callee when running
the retained wrappers, preserving an independent oracle. Modifier aliases cover
full and partial overlaps with constants, selected cells and begin/end pointers.
Prologue aliases include initially uncommitted root/descriptor pointer values
that become valid through the original saved-LR write. Rejections must precede
any traversal of those old values. A further 4,448 executions disable the switch
and compare production composition; 432 explicitly exercise a retained outer
caller with a native nested limit. Only
the unused 16-byte nested scratch is normalized in those mixed cases. These
tests do not establish the surrounding gameplay role or every sticky FP status bit.

`percentage_value_equivalence` compares both percentage entries with independent
retained nested callees in 8,352 cases: 4,783 native and 3,569 complete retained
chains. Native cases normalize only their untouched full nested scratch; retained
cases compare every byte and GPR/FPR. Comparisons cover all rounding/starting
flush modes, signed conversion boundaries, altered constants, carry, both enable
flags, current-value extremes, unaligned records, prologue/source aliases and
exact unmasked arithmetic. Flags and percentage fields deliberately overlap
getter scratch in cases where a mixed retained/native chain changes results.
Sixteen follow-up probes establish that the scoped retained-chain override does
not leak into later native calls. Portable fixtures include a double-rounding
counterexample, negative delta/carry, all clamps, inactive unused sources and
late transactional rejection.

`native_capped_accumulator` checks editable percentages and cap, signed wrap,
negative increments, positive saturation, enabled/disabled accounting, 64-bit
carries, integer rounding and rejected-trial rollback. `capped_accumulator_equivalence`
compares the actual translation in 2,064 cases, including 1,152 native paths.
It covers null/disabled/enabled owners, source/stack/write aliases, already-read
source aliases, pointer wrapping, exceptional sources, late conversion failure,
directed overflow and exact unmasked cases. All GPR/FPR results and whole fixture
bytes match, normalizing only the untouched 16-byte scratch on native paths.
Indirect registration and condition/carry/FP controls also match. These are
synthetic state checks, not matched-scene gameplay or performance validation.

The new `native_handle_table`, `native_state_payload` and `native_payload_install`
fixtures exercise bounded lookup/key rules, editable payload fields, optional
vectors, signed products, relation/category gates, signed zeros and rejection
without publication. `handle_table_equivalence`, `state_payload_equivalence` and
`state_relation_equivalence` are the corresponding retained-translation checks.
Handle comparisons cover 3,584 calls over all rounding/flush/exception-mask
combinations, including 1,024 native packed validations and 768 retained wrapper
chains. They compare all scalar registers and condition fields, unaligned
records, output/source overlaps, key/index boundaries, malformed retained
layouts, the full frame and separate argument homes. Read-only queries reject
unknown bounds without mutating output; uncommitted old pointers test guards
before dependent reads. Integer handle code preserves FP controls and status.

Payload comparisons cover 2,064 cases, including 1,344 native copies, all field
groups, all eight scalar-source lanes, directed rounding, signed zeros, arbitrary
integer bits, unaligned fields, adjacency and both overlap directions. They
compare every fixture byte and scalar register. Outer relation comparisons cover
4,896 cases, including 3,312 native calls, explicit wide positive/negative
products, sentinel skips and category gates. They test copy/vector/related/table
aliases, prologue-overwritten pointers, unmasked exact calls, pointer upper halves,
return/condition/carry results and FP controls. Native installer results compare
all scalar registers; native table-value results compare their contracted
GPRs, `f0/f13`, preserved `f1`–`f4` and nonvolatile registers. Only untouched
240-byte scratch is normalized for native outer calls; retained chains compare
every byte and scalar register. Retained scopes must release before later native
calls. Neither scalar-transferring native path claims every sticky FP status bit.

`native_direction_math` checks the reduction tree, every normalization stage,
threshold edits, signed zeros/DAZ, directed square-root rounding, unused fourth
components, overflow and transactional rejection under all rounding/host flush
combinations. `native_direction_accumulation` checks editable direction/state,
both selections, double-product/float-add boundaries, generated subnormals and
rollback.

`direction_normalization_equivalence` compares 4,640 calls (3,400 native and
1,240 retained). `direction_accumulation_equivalence` compares 3,072 calls
(1,440 native). Both check every fixture byte, including native scratch and
argument-home writes, all 32 GPRs/FPRs, all 128 VMX registers, all condition fields,
LR/CTR/XER, preserved state, indirect registration and FP controls. Fixtures cover
all rounding/starting flush modes, unaligned sources/outputs, safe overlaps,
scratch aliases, exceptional values, underflow/overflow, unmasked exact calls and
explicit host/guest control divergence. Sticky exception flags are outside the
comparison; no scratch is normalized away for these two entries.

The direction fixtures additionally compare high physical source/output/stack
addresses, every source alignment, physical aliases, upper pages and shadow-boundary
fallback. Independent test address mapping and unchanged unbiased-memory canaries
prevent the implementation's address helper from serving as its own oracle.

`native_direction_basis` checks the phased interface, deferred unused operands,
signed axes and basis orientation, edited constants, each cross/weighted stage,
independently rounded integer inputs, scalar subnormals and final DAZ, altered
fourth lanes and early/late transactional rejection. `direction_basis_equivalence`
compares 4,672 calls (3,728 native and 944 retained), plus 384 retained outer calls
using the production normalizer. A switch compiled only into this test forces
all nested normalizers retained in the independent oracle. Every fixture byte,
all GPR/FPR/VMX registers, condition fields and FP/control state are checked.
Coverage includes wrapping slots, overwritten prologue pointers, independent
host/guest rounding, high physical records and slots, shadow canaries, arbitrary
initial output bits, unaligned final stores and wrapped raw fourth lanes. Its
physical cases include 168 native and 88 retained routes. No original scratch is
removed from these comparisons. Sticky FP status remains outside the contract.

Physical-shadow fixtures currently cover the three direction adapters. Forty
older memory-backed typed entries still require mapped accesses, alias/range
guards and corresponding high-address fixtures. Their earlier comparisons
establish virtual-address behavior only; this remains part of the conversion work.

Both Windows configurations pass forty suites. A 24-second isolated
Direct3D Release startup reaches menu installation and frame pacing without
logged errors; Windows EXE/plugin privacy scans pass. These are synthetic and
startup checks, with no additional gameplay or performance claim.

## Adding the next module

Identify a bounded engine routine and its inputs, outputs, units, side effects,
ownership and thread. Keep the original translated entry available. Write the
portable model first, then a narrow guest-memory adapter. Compare its results
against the original on supported data, preserve unknown cases, and validate
the affected gameplay route before claiming compatibility or a speedup.
