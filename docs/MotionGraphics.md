# Motion graphics (Parallax MG)

This doc is about the **2D MG layer in Parallax** and how **MovieMaker (SMM)** edits it. It is not a full guide to `Solstice::UI::MotionGraphics` widget helpers (animated buttons, etc.); for easing math and shared keyframe types see **[MinGfx.md](MinGfx.md)** and the UI sources under `source/UI/Motion/`.

---

## Parallax MG + SMM

**LibParallax** stores separate **MG elements** and **MG tracks** (like 3D channels). Evaluating the scene fills an **`MGDisplayList`** via **`EvaluateMG`** (`ParallaxEvaluate.cxx`). **MovieMaker** authors layout in **screen pixels**, uses a **nominal comp size** (alignment math only; export resolution is configured elsewhere), and shares the same **Curve editor** as 3D—including INI keyframe presets from `presets/Keyframe/` (**[Presets.md](Presets.md)**). Which SMM panels and menus touch MG (Properties, export presets, preview vs video path) is summarized in **[SMM.md — 2D motion graphics (MG) in SMM](SMM.md#2d-motion-graphics-mg-in-smm)**; this page is the technical reference.

Root-level entries in the list are composited in ascending **`Depth`** (larger = drawn later / more in front). Missing **`Depth`** is **0**. Stacking is 2D only unless a future pipeline shares depth with 3D.

**Paths:**

| Path | What runs |
|------|-----------|
| CPU raster | [`RasterizeMGDisplayList`](../SDK/LibParallax/include/Parallax/MGRaster.hxx): sprites, stb text, per-sprite FX, then [`ApplyMGPostProcessRgba`](../SDK/LibParallax/include/Parallax/MGRaster.hxx) (chromatic aberration, color grade). Used by MovieMaker’s static 2D preview and LibParallax frame export when `IAssetResolver` is set. |
| ImGui compositor | [`MotionGraphicsCompositor.cxx`](../source/UI/Motion/MotionGraphicsCompositor.cxx): quads + text. Applies **screen shake** as a layer origin offset. **Per-sprite** chroma / rotoscope / zoom blur are **not** in this path. |
| ffmpeg (SMM) | Renders the compositor to an FBO, then **`ApplyMGPostProcessRgba`** on the readback so **grade + CA** match the CPU look. |

**Caveat:** The ImGui path does not run the multi-sample sprite shader logic; for **exact** WYSIWYG of sprite FX, use the CPU preview or the final encode after readback.

---

## Schemas and attributes (authoring / tracks)

Property names in MG tracks must match the schema attribute name string-for-string.

### `MotionGraphicsRootElement`

| Attribute | Type | Role |
|-----------|------|------|
| `CompositeAlpha` | float | Global opacity of the evaluated MG layer. |
| `MGTimeScale` | float | **Speed ramping (linear):** `evalTick = timeTicks * MGTimeScale` for sampling **all** MG tracks. **Not** a spline time-remap; animate this float to change speed over the timeline, but each frame is a single scale factor, not a curve look-around sample. |
| `ScreenShakeAmpX`, `ScreenShakeAmpY` | float | Pixel amplitude (sin/cos wobble, see below). |
| `ScreenShakeFrequency` | float | Multiplies the internal time `0.01f * timeTicks` inside the wobble. |
| `ScreenShakePhase` | float | Phase offset. |
| `ChromaticAberration` | float | RGB split in **pixels** (full-frame post). |
| `GradeExposure` | float | Multiplier on RGB before sat/contrast (default 1). |
| `GradeSaturation` | float | 0 = grayscale, 1 = as-is, >1 more saturated. |
| `GradeContrast` | float | Scaled around 0.5 in normalized space (default 1). |
| `GradeLift` | float | Additive bias after contrast. |

**Screen shake (evaluation):** `offsetX = AmpX * sin(Frequency * 0.01f * timeTicks + Phase)` and `offsetY = AmpY * cos(Frequency * 0.01f * timeTicks + Phase * 0.5f)` (see `ParallaxEvaluate.cxx`). The same numbers shift the **draw origin** in MGRaster and the compositor.

**Post order (CPU):** composite all layers → `ApplyMGPostProcessRgba` (R from left, G center, B right per pixel for CA, then simple lift / contrast / sat). Supersample path runs post on the high-res buffer before 2× box downscale.

### `MGSpriteElement`

Sprites use **`Texture`**, **`Position`**, **`Size`**, **`Color`**, rotation, smear, UV scroll/wave, AO (see SMM 2D panel), plus:

| Attribute | Type | Role |
|-----------|------|------|
| `ChromaKeyColor` | Vec3 (linear 0–1) | Key color in **texture** space. |
| `ChromaKeyTolerance` | float | **0 = off.** Euclidean distance in RGB below this makes the texel fully transparent. |
| `ChromaKeyFeather` | float | Soft transition above tolerance. |
| `RotoscopeStrength` | float | Darken pixels where **neighborhood alpha** changes (ink-on-edge; not full traditional rotoscoping). |
| `RotoscopeEdgePx` | float | Scales how far we look for alpha gradients (texture UV scale). |
| `ZoomBlur` | float | 0 = off, up to 1. Radial / zoom-style blur: multi-sample from blur center toward the current texel. |
| `ZoomBlurCenterU`, `ZoomBlurCenterV` | float | Blur center in 0–1 **texture** space (defaults 0.5, 0.5). |

### `MGTextElement`

`Text`, `Position`, `Color`, `Depth`. stb easy font on CPU; compositor uses ImGui `AddText` with the same layout offset constants as MGRaster (`kRootOrigin` + optional shake only on CPU/ImGui origin).

---

## API touchpoints (files)

- **Scene / binary:** `ParallaxScene.hxx`, `ParallaxBinary.cxx` — MG section in `.prlx` (or your container).
- **Evaluation:** `ParallaxEvaluate.cxx` — `EvaluateMG`, `MGTimeScale`, post fields on `MGDisplayList::Post`.
- **Types:** `ParallaxTypes.hxx` — `MGDisplayList`, `MGPostProcessSettings`.
- **CPU raster + post:** `MGRaster.cxx` / `MGRaster.hxx` — `RasterizeMGDisplayList`, `ApplyMGPostProcessRgba`.
- **ImGui draw:** `MotionGraphicsCompositor.cxx` / `.hxx` — `Compositor::Submit` / `Render`, sprite quads, shake on origin.
- **SMM UI:** `SmmMg2DPanel.cxx` — comp tools, **root** when `MotionGraphicsRootElement` is selected, sprite FX when a sprite is selected.
- **TSV / sidecar import:** `SmmSessionAuthoring.cxx` — `StringToValueForKey` for float/vec3 property names.
- **Video export:** `VideoExport.cxx` — `ApplyMGPostProcessRgba` after `glReadPixels`.

For utilities packaging and where ffmpeg sits, see **[Utilities.md](Utilities.md)** and **[SMM.md](SMM.md)**.
