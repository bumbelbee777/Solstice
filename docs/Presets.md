# User presets (INI)

Solstice tools can load small **`.ini` files** from a **`presets/`** tree next to the **executable** and next to the **active project** (the folder that contains the `.smm.json` file). The reader is a **minimal** line-oriented parser in **LibUI** (`LibUI::Ini::ParseFile`): `[Section]` headers, `key=value` pairs, and `#` / `;` comments.

**MovieMaker (SMM)** currently uses this for **keyframe curve presets**; the same search roots can be extended later for comp sizes, color labels, and other short metadata without bumping the `.smm.json` schema.

## Search order

1. **`<MovieMaker.exe directory>/presets/`** (copied on build from the repository `presets/` when present)  
2. **`<parent of your .smm.json>/presets/`** (per-project overrides and team-shared files)

Subfolders group preset kinds; keyframe files live in **`presets/Keyframe/*.ini`**.

## Keyframe curve presets

Each file may define several blocks. A block **must** be named:

`[KeyframeCurvePreset:your_id]`

| Key | Meaning |
| --- | --- |
| `DisplayName` | Optional label in the **Curve editor** combo (defaults to `your_id`) |
| `EaseIn` | 0–13: `Linear` through `Bezier` (MinGfx `EasingType`, ease **into** the key) |
| `EaseOut` | 255 = **inherit** the next key’s ease on the **outgoing** segment; else 0–13 |
| `Interp` | 0 = parametric Eased, 1 = Hold, 2 = Linear, 3 = Bezier (float 1D cubic) |
| `TangentIn` / `TangentOut` | 0.02–0.99 when `Interp=3` (float Bezier handles) |

Use **Apply INI keyframe preset to selected** in the **Curve editor** (with keys on the current track) to copy these values into the Parallax/MG keyframes. This applies to **3D (element channels)** and **2D (MG tracks)** the same way.

## Example

See the repository `presets/Keyframe/soft_in.ini`.

## Parser limits

- No multiline values, no includes, no array syntax.  
- Keys and values are trimmed; UTF-8 text is not normalized beyond that.

For the **full SMM** workflow, see [SMM.md](SMM.md) and [MotionGraphics.md](MotionGraphics.md).
