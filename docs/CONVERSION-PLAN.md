# Readable C++ game-code conversion

The game already builds its generated C++ translations into native machine code.
The conversion work replaces PowerPC register/memory-oriented implementations
with typed C++ operations and explicit state that can be inspected, tested and
modified independently of the Xbox runtime. Python build/release scripts are
separate from game logic and remain part of the existing development toolchain.

## Current coverage

The codegen partition has 13,762 function assignments across 87 source files.
Those bodies comprise 13,525 `sub_` routines and 237 named helpers. The runtime
table adds 151 import mappings, for 13,913 address mappings overall. These
inventories cover different boundaries. Development currently has 44 typed native
entry points, grouped below and checked against the retained translations.
These counts are not
a percentage of game logic or work completed: functions differ greatly in size,
some entries share an implementation, and unsupported inputs still use retained
translations. Runtime/UI hooks are not counted as complete engine conversions.

| Group | Entry points | Native interfaces |
| --- | ---: | --- |
| Frame clock | 1 | Frequency, timestamp, target rate and deadlines |
| Bounds and projections | 9 | Sphere/capsule/box geometry and transforms |
| Vector updates and cached channels | 8 | Explicit three/four-component values and conditional direction accumulation |
| Direction normalization | 1 | Squared-length stages, threshold decision and separate reciprocal refinement |
| Countdown, scalar, bit and ratio operations | 5 | Counters, arithmetic inputs and bit windows |
| Delay/mixing and feedback processing | 7 | Taps, gains, histories, cursors and output caches |
| Table-derived limits and bounded values | 8 | Channel/category selection, parallel and related table values, percentage changes, modifier replacement and lower/upper comparisons |
| Capped accumulation and accounting | 1 | Scaled increments, signed wrapping/capping and optional total/count state |
| Handle lookup and state payloads | 4 | Bounded records, packed-key validation, typed field transfer, optional vectors and related-object multiplication |

[Native contracts and validation](NATIVE-CODE.md) identify the addresses,
supported inputs, guest adapters and equivalence checks. [Status](STATUS.md)
records observed gameplay coverage separately from synthetic tests.

## Work toward full coverage

1. Continue bounded state/math and sample-processing routines. Trace their
   callers, identify ownership and layout, and compare native results with the
   retained translations before switching each entry.
2. Reconstruct animation, motion, combat and scripted-event update boundaries.
   Establish units and original-speed behavior from matched scenes before
   introducing higher-FPS timing changes.
3. Replace resource, mission, UI and save-related behavior through typed
   interfaces, preserving file formats and existing progress. Validate concrete
   gameplay routes as each larger subsystem becomes understood.
4. Audit remaining generated call targets and unsupported-case fallbacks. Remove
   a dependency on a translated path only after its contract and supported game
   behavior have corresponding coverage. Keep a record of unresolved cases.

The connected handle/payload batch exposes the record and relation interfaces
needed by larger callers. Direction normalization now exposes the exact rounded
math used by 147 translated callers (240 direct call sites). Its three uses in
`0x8209D9B0` provide a boundary for the next direction-basis conversion; that
larger caller remains translated. Call-site counts establish static reach,
not observed gameplay execution or a performance gain.
Broader progression and callback-driven state changes
remain translated until their ownership, side effects and callback contracts
have explicit interfaces; the extracted arithmetic does not identify those
systems' gameplay roles.

Each batch needs meaningful portable fixtures, comparison with the actual
translation, calling-convention and memory checks, both Windows configurations,
and portable component CI. Direct3D startup and privacy checks accompany runtime
milestones. Passing those checks does not establish full-game compatibility,
performance gains or a playable build on another operating system.
