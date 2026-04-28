# User presets (INI)

Solstice tools can load small **`.ini` files** from a **`presets/`** tree next to the **executable** and next to the **active project** (the folder that contains the `.smm.json` file). The reader lives in **LibUI** (`LibUI::Ini::ParseFile`): `[Section]` headers, `key=value` pairs, `;` comments, and `#` **comments** — **except** lines that start with `#include` (see below).

**MovieMaker (SMM)** uses this for **keyframe curve presets** and **timeline / shot range presets** (nested sub-timeline). The same search roots apply to both; no `.smm.json` schema bump is required.

## Search order and overrides

1. **`<MovieMaker.exe directory>/presets/`** (copied on build from the repository `presets/` when present)  
2. **`<parent of your .smm.json>/presets/`** (per-project overrides and team-shared files)

If the same **`KeyframeCurvePreset:<id>`** (or **`TimelineRangePreset:<id>`**) appears in more than one file, **later roots win** — the project `presets/` folder overrides shipped defaults for the same id. Within one root, **later files** (filesystem order) override earlier when ids collide.

## Keyframe curve presets

Files: **`presets/Keyframe/`** and **any subfolders** (e.g. `presets/Keyframe/Act1/foo.ini`).

Each file may define several blocks. A block **must** be named:

`[KeyframeCurvePreset:your_id]`

| Key | Meaning |
| --- | --- |
| `DisplayName` | Optional label in the **Curve editor** combo (defaults to `your_id`) |
| `Description` | Optional; shown in the curve panel for handoff notes |
| `Author` / `Tags` | Optional metadata; `Tags` is a simple string (commas, spaces) used for **filtering** the preset list |
| `EaseIn` | 0–13: `Linear` through `Bezier` (MinGfx `EasingType`, ease **into** the key) |
| `EaseOut` | 255 = **inherit** the next key’s ease on the **outgoing** segment; else 0–13 |
| `Interp` | 0 = parametric Eased, 1 = Hold, 2 = Linear, 3 = Bezier (float 1D cubic) |
| `TangentIn` / `TangentOut` | 0.02–0.99 when `Interp=3` (float Bezier handles) |

Use **Apply INI keyframe preset to selected** in the **Curve editor** (with keys on the current track) to copy these values into the Parallax/MG keyframes. This applies to **3D (element channels)** and **2D (MG tracks)** the same way. The panel supports **filter**, **Reload INI** (picks up edits on disk), and the metadata above for collaboration.

## Timeline / shot range presets (SMM)

Files: **`presets/Timeline/`** (recursive, same as keyframe). Blocks:

`[TimelineRangePreset:your_id]`

| Key | Meaning |
| --- | --- |
| `DisplayName` | Optional label in the main-timeline “Shot / act ranges” control |
| `StartTick` / `EndTick` | Scene ticks; **EndTick** is **exclusive** (same as the nested *end* field in the UI) |
| `Description`, `Author`, `Tags` | Optional; same spirit as keyframe metadata |

In the main layout, pick a range and use **Apply to nested sub-range** to map that window onto the full timeline view for detailed animation; **Jump playhead to range start** moves the playhead. See [SMM.md](SMM.md#collaboration-and-long-form-animation) for how this helps long projects.

## Example keyframe file

See `presets/Keyframe/soft_in.ini`.

## Example timeline file

See `presets/Timeline/first_beat.ini`.

## Parser features (LibUI)

- **Line continuation:** a backslash at the end of a **trimmed** physical line continues on the next line (handy for long `Description=…` text).  
- **Includes:** `#include "relative/path.ini"` or `@include "relative/path.ini"` — resolved relative to the **including file’s** directory. Cycles and depth (default max 32) are capped; errors are reported in the optional error string.  
- `#` is a comment only when the line is **not** an include directive.  
- Not supported: true multiline values without backslash, array syntax, or explicit UTF-8 normalization beyond trim.

For the **full SMM** workflow, see [SMM.md](SMM.md) and [MotionGraphics.md](MotionGraphics.md).
