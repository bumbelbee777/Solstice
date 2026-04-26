# SMM (Solstice Movie Maker) workspace

SMM (`MovieMaker`) is the **Parallax authoring tool**: you import assets, edit a timeline-driven **3D + motion-graphics** scene, and export **`.prlx`** (and optionally video via ffmpeg). The UI is **viewer-first**—a large central preview—with scene and asset controls on the left and a **timeline** below, similar in spirit to compact film-motion (SFM-style) layouts.

If you are new here: **`.smm.json`** is the **project bookmark** (paths, preferences). **`.prlx`** is the **actual scene** (elements, channels, MG layers). You can work with either file type; saving the project updates both when configured.

## Workspace model

- The **Authoring** tab (left column) holds **Technology Preview 1** tooling that does **not** change the **`.smm.json` `version`: 1** line or the Parallax **on-disk format** version: a separate **`.smm.authoring.tsv`** next to the project file stores a **simplified asset database** (hash, path hint, tags, **proxy** flag, optional **resolves-to** hash), **prefab** definitions (`Schema=...; Key=val;…` rows), and **lipsync line stubs**. **Save Project** also writes that TSV (and it is reloaded when the project path changes). Use **Save / Reload** in the tab for manual round-trips.
- The top bar uses normal app menus: **File**, **Edit**, **View**, and **Help**.
- The **main viewer** focuses on a **unified viewport** (default): one panel composites **schematic 3D** (lit cubes from the scene graph) with a **2D motion-graphics** layer on top, so you see **composite framing** without switching tabs. Legacy **3D** / **2D MG** tabs may still be available from **View** for side-by-side debugging.
- **View** also toggles optional **dock panels**: **curve editor**, **graph editor**, and **particles**—these are part of SMM’s **VFX-oriented** workflow (see below).
- **Properties** and **Assets** live in the left sidebar so the viewer and timeline stay visible without long vertical scrolling. **Properties → Environment / skybox (Scene root)** edits Parallax **`SceneRoot`** fields for a **cubemap sky** (the same +X/−X/+Y/−Y/+Z/−Z face path idea as [Jackhammer’s `.smf` skybox](Utilities.md#jackhammer-leveleditor-smf)). **Arzachel, LOD, animation presets** appear when you select an **`ActorElement`**: **rigid-body damage** (0–1) is the `Arzachel::Damaged(..., amount)`-style amount for prop destruction variants; **LOD** distances are authoring hints; **Animation clip preset** and **Destruction / breakup preset** are string labels to pair with the mesh **`AnimationClip`** asset and breakup content. All of these are **saved in `.prlx`** and show up in **`EvaluateScene`** (`EnvironmentSkybox`, `ActorArzachelAuthoring`).
- The **animation timeline** supports horizontal scrolling for long or zoomed timelines, and vertical scrolling when many tracks or keyframes exceed the visible rows. With the timeline canvas focused, **Ctrl+mouse wheel**, **Ctrl+Plus** / **Ctrl+Minus** (numpad **+** / **−** too), and **Ctrl+0** adjust **zoom** (slider range 0.1×–32×); the status line next to the zoom slider summarizes the shortcuts. It stays in sync with optional curve/graph panels via a shared **bridge** to Parallax channels and MG tracks.
- **Nested sub-timeline:** below the main timeline strip, enable **Nested sub-range** and set **start** / **end (exclusive)** scene ticks (or use the quick buttons). The track area then maps that `[start, end)` window to the full width so you can edit a “shot” without changing the overall scene duration. The **curve editor** uses the same mapping for the horizontal axis; the playhead stays in **absolute** scene ticks. Clear the checkbox to return to the full timeline view.

## Animation workflow (DCC- and Sequencer-style)

SMM is not a full replacement for **Maya** (rigging, nodes, Bifröst, etc.), **Houdini** (proceduralism, VEX), or **Unreal** (Control Rig, Sequencer, animation BP)—but the Movie Maker build aims at a **credible, viewer-first** animation stack for **Parallax**: timeline keys, f-curves, lightweight **drivers**, and production export. Use this section to see what is in scope today versus common expectations from large tools.

| Area | Industry tools (typical) | SMM (Parallax / MovieMaker) |
| --- | --- | --- |
| **Time model** | Global timeline, sub-shots, take system | **Scene ticks** + **nested sub-range** (shot-in-shot zoom on the same timeline) + **loop region** in session (see workflow / playback) |
| **F-curves** | Bezier or stepped tangents, Euler/Quaternion | **Parametric easings** on the **segment into** each key (MinGfx `EasingType` on the *destination* key) plus **outgoing ease** on the *source* key (`0xFF` = inherit the next key’s ease-in for that segment). **Interpolation mode** on the *destination* key: **Eased** (parametric lerp), **Linear**, **Hold/Step** (value stays at the previous key until the destination key’s tick), or **Bezier** (1D value-space cubic on **float** channels, with per-key **tangent in/out** weights). The curve panel samples the same rules as `EvaluateChannel` / `EvaluateMG` (Parallax 1.0 on-disk, **no** separate format version bump is required to use these keyframe fields in current builds). |
| **Easing / tangency** | Per-tangent handles (weighted bezier) | **Curve editor:** ease-in, **ease-out (segment leaving this key)**, **interpolation** + **Bez tangents** (sliders, **Load from selected**, **Auto smooth** for a 1/3-style default), **Zoom value to fit** (value axis, with reset), and optional **INI keyframe presets** from **`presets/Keyframe`**. A warning appears when any synced float track has more than **20k** keyframes (UI cost). |
| **Constraints / rigging** | IK, aim, full constraint stacks | **Graph editor** links (**driver → driven**, scale/offset) with **bake** at the playhead (expression-light workflow) |
| **Retargeting / skeleton** | HumanIK, retarget manager, control rig | **Arzachel** presets and mesh **`AnimationClip`** on actors are **authoring metadata**; full skeletal solve is an engine/runtime concern |
| **Procedural** | Houdini CHOPS, SOPs | **Particles** (multi-stop color over life in preview) + Parallax **MG** + optional **fluid** preview |
| **Output** | Alembic, USD, fbx, game build | **`.prlx`** (Parallax) + **ffmpeg** video + recovery snapshots |

**Recent additions (curve UX):** per-key **easing** is editable in the **Curve editor** (not only via copy/paste). The **curve canvas** follows **hold**, **linear**, **float Bezier** (cubic in value), and **parametric eased** segments so the plot matches playback. **User INI keyframe presets** (see [Presets.md](Presets.md)) are resolved from the **MovieMaker** directory and the **project** folder and can be applied to selected keys. **2D (MG):** the **Properties** tab includes an **After Effects–style** nominal comp size, **nudge** / **align** for sprites, **link W/H** for uniform scale, and **root CompositeAlpha** when a **MotionGraphicsRoot** element is present; see [MotionGraphics.md](MotionGraphics.md#parallax-mg--smm) for Parallax behavior.

## Unified viewport and authoring tools

The **unified viewport** is the main differentiator for day-to-day authoring:

- **Shared camera**: orbit, pan, zoom, and **projection** (perspective or orthographic top/front/side) persist while you work. **Reset view** snaps the camera back to a sensible default.
- **MG overlay**: a slider blends the CPU-rasterized **motion-graphics** layer over the 3D capture (0 = 3D only, 1 = full composite). The overlay label summarizes **MG root count**, **sprite** count (nested `MGSpriteElement` nodes), live **particle** preview count, and short flags when **`.smat`** or **particle texture** preview is active.
- **MG depth (2D sort)**: each **`MGTextElement`** and **`MGSpriteElement`** can carry a float **`Depth`** (default 0). When the MG layer is rasterized or drawn in **video export**, root MG nodes are composited in **ascending Depth**—**larger values paint on top**. This is **screen-space draw order** (like a z-index), not a depth buffer against the 3D pass: the unified viewport still builds **3D RGB** first, then **alpha-blends the whole MG plane** using the MG overlay slider. Use **Depth** (or keyframe it on a track named `Depth`) to force HUDs, subtitles, or sprites to stack predictably when they overlap.
- **Schematic 3D** still comes from **`EditorEnginePreview`** (offscreen bgfx + `EvaluateScene`), with gizmos for elements and lights. **Framing guides** (checkbox in the viewer chrome, **Framing guides (unified view)**) draw a **title/action safe** rectangle (10% inset), **rule-of-thirds** lines, and a **center cross** on the **letterboxed** preview—useful for blocking like TV safe areas and composition.
- **Viewport shortcuts:** **Shift+click** in the unified viewport **picks** the actor/camera (and similar evaluated elements) by ray against the same AABB gizmo size used for drawing; small mouse movement after press still counts as a click. **F** (while the viewport is focused) **frames the orbit** on the **selected** element’s transform when it appears in `EvaluateScene` (cameras/actors). The bottom status line in the viewer summarizes the Parallax **scene** (element/channel counts) and the first **validation** issue (e.g. fluid resolution over budget) when present. **View → Fluid volumes** opens the **fluid** panel; enable **Fluid AABB overlay** in the material/preview block to see authored fluid bounds in the 3D view (authoring only, like Jackhammer’s `FLD1` grid parameters).
- **3D selection outline (LibUI)**: the **selected** evaluated element (ray-picked or from the outliner) is drawn with a **double** wireframe pass—dark **outer** halo + **bright** inner lines—so it reads as a “focus” outline in the letterboxed 3D view (see `DrawWorldAxisAlignedBoxSelectionOutlineUniformImGui` in `utilities/LibUI/Viewport/ViewportGizmo.*`).
- **Curve editor** (dock): keyframes for **float** and **per-component Vec3** channels. **Easing in** (segment ending at the key) and **ease out** (segment *after* the key, or inherit) plus **interpolation** and **Bez tangents** are edited in the panel (**Apply to selected**; multi-key supported). **Time snap to frame grid** (set frame rate, e.g. 24/30/60, or 0 to disable per-axis) applies when you **add** (double-click) or **move** keys. **Ctrl+C** copies the **selected** key; **Ctrl+V** **pastes** at the **playhead** on the **current** track, preserving the copied **easing** where supported. **Del** removes the **selected** key or **all keys in a multi-selection**. **Shift+click** toggles keys in a **multi-selection** on the active track. The **Batch** row (under the canvas) can **nudge time** (ticks), **nudge value**, or **scale keyframe times** around the **current playhead** for all selected keys (time nudge / scale remove and re-add keys so indices stay consistent). The curve panel shares the timeline’s selected track and respects **nested sub-timeline** mapping when enabled; the **graph** between keys is drawn to match the runtime (including **hold** steps and **float Bezier** where enabled).
- **Graph editor** (dock): **driver → driven** links (scale/offset) with **Bake at playhead**; the selected link shows a **readout** of the **driver** value and the **scaled+offset** value the bake will **write to the driven** track at the current time.
- **Particle editor** (dock): CPU **billboard** preview; optional **multi-stop** **color over life** (2–6 RGBA stops along normalized lifetime, with **Sort stops**). **Write emitter to scene** still maps the **first** and **last** stops to Parallax `ColorStart` / `ColorEnd` (the extra stops are for **SMM preview**). Legacy **Color start** / **Color end** apply when the multi-stop mode is off. When the **Particles** panel has focus, **Edit → Undo / Redo** (and **Ctrl+Z** / **Ctrl+Y** / **Ctrl+Shift+Z**) apply to the **particle editor state** instead of the Parallax scene snapshot; scene undo still applies when another panel is focused.

## Materials and textures in preview

SMM can approximate **engine materials** while you block out a shot:

- **`.smat` files** (Solstice material v1) can be assigned to **preview cubes** for schematic elements. Use **Actors only** / **Selected element only** to limit which cubes get the material. Indices inside `.smat` that do not exist in the preview texture registry are **ignored** so you still see **base color** and parameters instead of broken sampling.
- **Raster maps** (optional): **albedo**, **normal**, and **roughness** images can be bound for the same preview pass so textures show up in the offscreen renderer, not just flat tints.

These paths are **authoring aids** for MovieMaker; full engine texture tables may differ at runtime.

## Particles and Parallax export

Particle settings are **exportable** into the **`.prlx`** file:

- The scene can contain a dedicated element **`SMM_ParticleEmitter`** (schema **`SmmParticleEmitterElement`**) under the scene root. It stores the editor parameters (spawn, lifetime, velocities, gravity, drag, sizes, colors, max count, attach flags) plus **`SpriteTexture`** (**`AssetHash`**) and **`SpriteSourcePath`** (**string**) when you use an imported sprite.
- **Saving or exporting** `.prlx` **automatically merges** the current particle editor state into that element before writing, so you do not have to remember a separate step.
- **Importing** a `.prlx` **reloads** the particle panel from that element when present.
- The **Particles** panel also offers **Write emitter to scene** / **Read emitter from scene** (with undo snapshot on write) if you want to push or pull without saving the file immediately.

Sprite **bytes** are brought into the session via **`DevSessionAssetResolver::ImportFile`** (same family of hashing as other MovieMaker imports). If a file only has a **hash** and no stored path, you may need to re-bind a file for **local preview**; the **hash + path** combination is what travels for tools and runtimes that resolve assets.

## Saving and export

- **File → Save Project** / **Ctrl+S** opens a native save dialog until a `.smm.json` project path is chosen, then saves to that path on later saves.
- Project files use `.smm.json` and currently write schema **`version` 1** for path/session metadata (Technology Preview 1; values evolve without bumping the number while we iterate). The project may also store **`recoveryIntervalSec`** (10–600), the interval for background **recovery** writes (see below).
- **PARALLAX** export writes `.prlx` through `LibParallax::SaveScene`, creates parent folders, updates recent paths, and supports optional **ZSTD** compression (uncompressed header, compressed tail—see format notes in utilities docs).
- **File → Export…** opens the export window directly (`.prlx` path, compression, video section).
- **Video export** uses the **ffmpeg CLI** (MP4/MOV, dimensions, FPS, tick range). ffmpeg is optional at build time; the UI indicates whether encoding is available.
- **Render queue:** in **Export**, configure a video job (output path, resolution, fps, tick range, container), then **Add current settings to render queue**. You can queue several jobs (e.g. alternate paths or ranges) and **Run render queue (sequential)** to encode them one after another without re-entering settings.

## Autosave recovery

- While the Parallax scene is **dirty**, MovieMaker periodically writes a **recovery snapshot** of the current `.prlx` bytes (with the same ZSTD option as normal save) into the editor **FileRecovery** store under a per-app key (`smm`). The **Export** window exposes a **Recovery interval (sec)** slider (also persisted in `.smm.json` as `recoveryIntervalSec`).
- **File → Write recovery snapshot now** forces an immediate recovery write (useful before risky experiments).
- On launch, if recovery files exist, the existing **restore from recovery** flow (when shown) can load those bytes into the session. Recovery is **not** a substitute for **Save Project** / **PARALLAX export**; it is a safety net against crashes.

## Assets and media

- Generic asset import uses the **dev-session asset resolver** (in-memory **hash → bytes** for the current session).
- **Raster images** can be imported for **MG sprites** (`MGSpriteElement` **Texture** attribute); common formats include PNG, JPEG, BMP, TGA, WebP, and others supported by the shared image decode path. **`Depth`** on sprites/text controls **2D stacking** within the MG layer (see **MG depth** above).
- **Audio** can be imported for **`AudioSourceElement`** rows (session-backed **AudioAsset**). In **Properties** (with an `AudioSourceElement` selected), you see the **`AudioAsset` hash** when assigned, a short **runtime** note, and **Volume** / **Pitch** with **undo** (scene snapshot) on change. The **waveform** strip supports **click to seek** and **drag to scrub** with **real-time** preview audio; while you **drag**, the **vertical playhead** tracks the **cursor** so scrubbing stays visually locked to the pointer. Use **File** or **Assets → Import audio** to bind bytes; export writes raw session **AudioAsset** bytes.
- **Video import:** **File → Import video to session…** reads the chosen file into the **dev-session asset resolver** (hash + bytes), the same way other media imports work. SMM does not embed a full clip editor; the import is for **reference**, compositing hooks, or tooling that resolves **session** assets. Decoding/trimming is expected to be handled by external **ffmpeg** workflows or future Parallax features.
- **glTF / glb** follows a **selected-Actor** workflow: add or select an **`ActorElement`**, then import to assign **`MeshAsset`**; export writes the current mesh asset back to `.gltf` / `.glb` when the bytes are in the resolver session.

## Prefabs, asset database, morpheme & lipsync placeholders

- **Prefabs:** define an **id**, **display name**, **Parallax schema** (`CameraElement`, `ActorElement`, `MGTextElement`, …), and **`Key=value` pairs** separated by semicolons (see **Properties** / Parallax attribute names). **Instantiate** adds the element or MG node to the **current** scene (with undo).
- **Simplified asset database:** rows mirror **session** imports (hash + label). Mark **proxy** when a row is a low-LOD or stand-in asset; set **resolves to** (hex hash) when you know the **full-quality** asset hash for reference (runtime swap is still an engine concern; SMM stores **metadata** only).
- **Morpheme-like tokens:** **Extract** in Authoring uses **word-shaped** runs (letters, digits, hyphen) as coarse “morpheme” **proxies**—in the spirit of **2002–03** game tooling (not a real morphological analyser). The **Authoring** tab shows **stats** (word count, unique tokens, character count, **vowel density**) to sanity-check the line.
- **Lipsync:** implemented in **`SmmLipsyncMorpheme`** + Authoring UI. **Mode 0** = legacy **even word windows** along `[StartTick, EndTick]`; **Mode 1** = **vowel-centered** sub-timing (MG **Depth** placeholder). **Mode 2** = **English letter → viseme** samples (OVR-style ids: `aa`, `PP`, `sil`, …) written to the selected **`ActorElement`** as Parallax channels **`FacialVisemeId`** (`String`) and **`FacialVisemeWeight`** (`Float`)—these drive **`EvaluateScene`** → **`ActorFacialPoses`** (Arzachel facial stack) for runtime/preview. **Apply to MGText** uses modes 0–1 only; **Apply viseme keys to selected Actor** uses the same stub span/text. **TSV** lines: `lipsync`, `label`, `startTick`, `endTick`, `text`, then optional `strength`, **`phoneticMode` (0, 1, or 2)**, `maxKeyframes`. **Railguards:** same caps/notes as before (text trim, max keyframes, **Start=End** single-sample). Stubs persist in the authoring TSV without bumping `.smm.json` **version**.

## API alignment

`SDK/SolsticeAPI/V1/Smm.h` provides a dedicated SMM API lane for:

- session state / playhead clamp (`SolsticeV1_SmmSessionState`, `SolsticeV1_SmmClampPlayhead`)
- viewer tab policy (`SolsticeV1_SmmViewerTab`, `SolsticeV1_SmmSetViewerTab`)
- export gating policy helpers for integrations (`SolsticeV1_SmmExportIntent`, `SolsticeV1_SmmCanRunExport`)

For Parallax schema names, motion graphics semantics, and timeline concepts, see **[MotionGraphics.md](MotionGraphics.md)** and **[Utilities.md](Utilities.md)** (SMM section). To ship a build, see **[Packaging.md](Packaging.md)** and [`MovieMaker.fst`](../tools/packaging/MovieMaker.fst) (optional **ffmpeg** in the same folder as the executable when configured).
