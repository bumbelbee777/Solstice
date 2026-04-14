# Solstice C API (version 1)

Stable **C** entry points for tools and hosts that load `SolsticeEngine` (Windows: `SolsticeEngine.dll`, Linux: `libsolsticeengine.so`). Umbrella header: [`SDK/SolsticeAPI/V1/SolsticeAPI.h`](../SDK/SolsticeAPI/V1/SolsticeAPI.h).

## Stability (MVP rules)

- Prefer **`SolsticeV1_*`** names. Legacy `Compile` / `Execute` / `Initialize` in the DLL are deprecated wrappers; new code should use [`Scripting.h`](../SDK/SolsticeAPI/V1/Scripting.h) and [`Core.h`](../SDK/SolsticeAPI/V1/Core.h).
- **`SolsticeV1_ResultSuccess`** means the operation completed. **`SolsticeV1_ResultFailure`** means the operation rejected input or hit an error path.
- **Error buffers:** When a function takes `ErrBuffer` / `ErrBufferSize`, null or zero size may still return failure but will not write text. On failure, a short message is copied with a trailing `NUL` when `ErrBufferSize > 0`. Multi-line messages use newline separators.
- **Output buffers:** If `OutBuffer` is too small, many APIs return failure and set an error string (for example `"Output buffer too small"`).
- **Evolution:** New functions may be added; existing `SolsticeV1_*` signatures should not change without a new versioned API family.

## Module index

| Header | Role |
| --- | --- |
| [`Common.h`](../SDK/SolsticeAPI/V1/Common.h) | `SOLSTICE_V1_API`, `SolsticeV1_ResultCode`, `SolsticeV1_Bool` |
| [`Core.h`](../SDK/SolsticeAPI/V1/Core.h) | Engine init/shutdown, version strings |
| [`Scripting.h`](../SDK/SolsticeAPI/V1/Scripting.h) | Moonwalk compile/execute (`SolsticeV1_ScriptingCompile` / `Execute`) |
| [`Narrative.h`](../SDK/SolsticeAPI/V1/Narrative.h) | Narrative JSON validate, JSON→YAML |
| [`Cutscene.h`](../SDK/SolsticeAPI/V1/Cutscene.h) | Cutscene JSON validate (same parse rules as runtime `CutscenePlayer`) |
| [`Smf.h`](../SDK/SolsticeAPI/V1/Smf.h) | `.smf` v1 binary validate |
| [`Audio.h`](../SDK/SolsticeAPI/V1/Audio.h) | Listener, emitters, music hooks |
| [`Physics.h`](../SDK/SolsticeAPI/V1/Physics.h) | World lifetime and stepping |
| [`Networking.h`](../SDK/SolsticeAPI/V1/Networking.h) | Relay/listen helpers |
| [`Fluid.h`](../SDK/SolsticeAPI/V1/Fluid.h) | Grid fluid sandbox API |
| [`Profiler.h`](../SDK/SolsticeAPI/V1/Profiler.h) | Counters and enable flag |
| [`MotionGraphics.h`](../SDK/SolsticeAPI/V1/MotionGraphics.h) | Easing helpers |

## Tests

- [`tests/CAPISmokeTest.cxx`](../tests/CAPISmokeTest.cxx) exercises the linked V1 API (including narrative, cutscene, and SMF validation). CTest label: `api` / `quick`.

## See also

- [Utilities](Utilities.md) — Sharpon and other tools that call these exports.
- [Narrative](Narrative.md) — dialogue/cutscene data shapes and Moonwalk natives.
