# SMM (Solstice Movie Maker) workspace

SMM (`MovieMaker`) is the **Parallax authoring tool**: you import assets, edit a timeline-driven **3D + motion-graphics** scene, and export **`.prlx`** (and optionally video via ffmpeg). The UI is **viewer-first**—a large central preview—with scene and asset controls on the left and a **timeline** below, similar in spirit to compact film-motion (SFM-style) layouts.

If you are new here: **`.smm.json`** is the **project bookmark** (paths, preferences). **`.prlx`** is the **actual scene** (elements, channels, MG layers). You can work with either file type; saving the project updates both when configured.

## Workspace model

- The **Authoring** tab (left column) holds **Technology Preview 1** tooling that does **not** change the **`.smm.json` `version`: 1** line or the Parallax **on-disk format** version: a separate **`.smm.authoring.tsv`** next to the project file stores a **simplified asset database** (hash, path hint, tags, **proxy** flag, optional **resolves-to** hash), **prefab** definitions (`Schema=...; Key=val;…` rows), **lipsync line stubs**, and a **`cinematic` row** (3D-schematic post: strength/depth/center for **screen-space chromatic aberration** scaled by depth, **TAA smear** blend, and **screen-fog dither** for the offscreen `EditorEnginePreview` pass). **Save Project** also writes that TSV (and it is reloaded when the project path changes). Use **Save / Reload** in the tab for manual round-trips. The **3D schematic post** sub-panel edits the same values live in the unified viewport.
- The top bar uses normal app menus: **File**, **Edit**, **View**, and **Help**.
- The **main viewer** focuses on a **unified viewport** (default): one panel composites **schematic 3D** (lit cubes from the scene graph) with a **2D motion-graphics** layer on top, so you see **composite framing** without switching tabs. Legacy **3D** / **2D MG** tabs may still be available from **View** for side-by-side debugging.
- **View** also toggles optional **dock panels**: **curve editor**, **graph editor**, and **particles**—these are part of SMM’s **VFX-oriented** workflow (see below).
- **Properties** and **Assets** live in the left sidebar so the viewer and timeline stay visible without long vertical scrolling. **Properties → Environment / skybox (Scene root)** edits Parallax **`SceneRoot`** fields for a **cubemap sky** (the same +X/−X/+Y/−Y/+Z/−Z face path idea as [Jackhammer’s `.smf` skybox](Utilities.md#jackhammer-leveleditor-smf)). **Arzachel, LOD, animation presets** appear when you select an **`ActorElement`**: **rigid-body damage** (0–1) is the `Arzachel::Damaged(..., amount)`-style amount for prop destruction variants; **LOD** distances are authoring hints; **Animation clip preset** and **Destruction / breakup preset** are string labels to pair with the mesh **`AnimationClip`** asset and breakup content. All of these are **saved in `.prlx`** and show up in **`EvaluateScene`** (`EnvironmentSkybox`, `ActorArzachelAuthorings`).
- The **animation timeline** supports horizontal scrolling for long or zoomed timelines, and vertical scrolling when many tracks or keyframes exceed the visible rows. With the timeline canvas focused, **Ctrl+mouse wheel**, **Ctrl+Plus** / **Ctrl+Minus** (numpad **+** / **−** too), and **Ctrl+0** adjust **zoom** (slider range 0.1×–32×); the status line next to the zoom slider summarizes the shortcuts. It stays in sync with optional curve/graph panels via a shared **bridge** to Parallax channels and MG tracks.
- **Nested sub-timeline:** below the main timeline strip, enable **Nested sub-range** and set **start** / **end (exclusive)** scene ticks (or use the quick buttons). The track area then maps that `[start, end)` window to the full width so you can edit a “shot” without changing the overall scene duration. The **curve editor** uses the same mapping for the horizontal axis; the playhead stays in **absolute** scene ticks. Clear the checkbox to return to the full timeline view.

## Animation workflow (DCC- and Sequencer-style)

SMM is not a full replacement for **Maya** (rigging, nodes, Bifröst, etc.), **Houdini** (proceduralism, VEX), or **Unreal** (Control Rig, Sequencer, animation BP)—but the Movie Maker build aims at a **credible, viewer-first** animation stack for **Parallax**: timeline keys, f-curves, lightweight **drivers**, and production export. Use this section to see what is in scope today versus common expectations from large tools.

| Area | Industry tools (typical) | SMM (Parallax / MovieMaker) |
| --- | --- | --- |
| **Time model** | Global timeline, sub-shots, take system | **Scene ticks** + **nested sub-range** (shot-in-shot zoom on the same timeline) + **loop region** in session (see workflow / playback) |
| **F-curves** | Bezier or stepped tangents, Euler/Quaternion | **Parametric easings** on the **segment into** each key (MinGfx `EasingType` on the *destination* key) plus **outgoing ease** on the *source* key (`0xFF` = inherit the next key’s ease-in for that segment). **Interpolation mode** on the *destination* key: **Eased** (parametric lerp), **Linear**, **Hold/Step** (value stays at the previous key until the destination key’s tick), or **Bezier** (1D value-space cubic on **float** channels, with per-key **tangent in/out** weights). The curve panel samples the same rules as `EvaluateChannel` / `EvaluateMG` and serializes with the current breaking Parallax format revision. |
| **Easing / tangency** | Per-tangent handles (weighted bezier) | **Curve editor:** ease-in, **ease-out (segment leaving this key)**, **interpolation** + **Bez tangents** (sliders, **Load from selected**, **Auto smooth** for a 1/3-style default), **Zoom value to fit** (value axis, with reset), and optional **INI keyframe presets** from **`presets/Keyframe`**. A warning appears when any synced float track has more than **20k** keyframes (UI cost). |
| **Constraints / rigging** | IK, aim, full constraint stacks | **Graph editor** links (**driver → driven**, scale/offset) with **bake** at the playhead (expression-light workflow) |
| **Retargeting / skeleton** | HumanIK, retarget manager, control rig | **Arzachel** presets and mesh **`AnimationClip`** on actors are **authoring metadata**; full skeletal solve is an engine/runtime concern |
| **Procedural** | Houdini CHOPS, SOPs | **Particles** (multi-stop color over life in preview) + Parallax **MG** + optional **fluid** preview |
| **Output** | Alembic, USD, fbx, game build | **`.prlx`** (Parallax) + **ffmpeg** video + recovery snapshots |

**Recent additions (curve UX):** per-key **easing** is editable in the **Curve editor** (not only via copy/paste). The **curve canvas** follows **hold**, **linear**, **float Bezier** (cubic in value), and **parametric eased** segments so the plot matches playback. **User INI keyframe presets** (see [Presets.md](Presets.md)) are resolved from the **MovieMaker** directory and the **project** folder and can be applied to selected keys. For **2D motion graphics** in MovieMaker, see **[2D motion graphics (MG) in SMM](#2d-motion-graphics-mg-in-smm)** below; Parallax schema, evaluation, and export paths are documented in [MotionGraphics.md](MotionGraphics.md).

## Unified viewport and authoring tools

The **unified viewport** is the main differentiator for day-to-day authoring:

- **Shared camera**: orbit, pan, zoom, and **projection** (perspective or orthographic top/front/side) persist while you work. **Reset view** snaps the camera back to a sensible default.
- **MG workflow toggle**: the viewer has a global **MG workflow** switch:
  - **Pure 2D**: legacy screen-space MG workflow (After Effects-like).
  - **Unified 3D**: `MGSpriteElement` rows with `MGProjectionMode=1` become world-space proxies in the same 3D pass, can be moved with 3D gizmo semantics, and can cast real scene shadows.
  - In **Pure 2D**, enable **Pure2D: disable 3D** to force an MG-only viewport (no 3D background capture), for a strict After Effects-style editing pass.
- **MG overlay**: a slider blends the CPU-rasterized **motion-graphics** layer over the 3D capture (0 = 3D only, 1 = full composite). In **Unified 3D** mode, world-space MG sprites are removed from this raster overlay path to avoid double-draw.
- **MG depth (2D sort)**: each **`MGTextElement`** and **`MGSpriteElement`** can carry a float **`Depth`** (default 0). When the MG layer is rasterized or drawn in **video export**, root MG nodes are composited in **ascending Depth**—**larger values paint on top**. This is **screen-space draw order** (like a z-index), not a depth buffer against the 3D pass: the unified viewport still builds **3D RGB** first, then **alpha-blends the whole MG plane** using the MG overlay slider. Use **Depth** (or keyframe it on a track named `Depth`) to force HUDs, subtitles, or sprites to stack predictably when they overlap.
- **Schematic 3D** still comes from **`EditorEnginePreview`** (offscreen bgfx + `EvaluateScene`), with gizmos for elements and lights. **Framing guides** (checkbox in the viewer chrome, **Framing guides (unified view)**) draw a **title/action safe** rectangle (10% inset), **rule-of-thirds** lines, and a **center cross** on the **letterboxed** preview—useful for blocking like TV safe areas and composition.
- **Viewport shortcuts:** **Shift+click** in the unified viewport **picks** the actor/camera (and similar evaluated elements) by ray against the same AABB gizmo size used for drawing; small mouse movement after press still counts as a click. **F** (while the viewport is focused) **frames the orbit** on the **selected** element’s transform when it appears in `EvaluateScene` (cameras/actors). The bottom status line in the viewer summarizes the Parallax **scene** (element/channel counts) and the first **validation** issue (e.g. fluid resolution over budget) when present. **View → Fluid volumes** opens the **fluid** panel; enable **Fluid AABB overlay** in the material/preview block to see authored fluid bounds in the 3D view (authoring only, like Jackhammer’s `FLD1` grid parameters).
- **3D selection outline (LibUI)**: the **selected** evaluated element (ray-picked or from the outliner) is drawn with a **double** wireframe pass—dark **outer** halo + **bright** inner lines—so it reads as a “focus” outline in the letterboxed 3D view (see `DrawWorldAxisAlignedBoxSelectionOutlineUniformImGui` in `utilities/LibUI/Viewport/ViewportGizmo.*`).
- **Curve editor** (dock): keyframes for **float** and **per-component Vec3** channels. **Easing in** (segment ending at the key) and **ease out** (segment *after* the key, or inherit) plus **interpolation** and **Bez tangents** are edited in the panel (**Apply to selected**; multi-key supported). **Time snap to frame grid** (set frame rate, e.g. 24/30/60, or 0 to disable per-axis) applies when you **add** (double-click) or **move** keys. **Ctrl+C** copies the **selected** key; **Ctrl+V** **pastes** at the **playhead** on the **current** track, preserving the copied **easing** where supported. **Del** removes the **selected** key or **all keys in a multi-selection**. **Shift+click** toggles keys in a **multi-selection** on the active track. The **Batch** row (under the canvas) can **nudge time** (ticks), **nudge value**, or **scale keyframe times** around the **current playhead** for all selected keys (time nudge / scale remove and re-add keys so indices stay consistent). The curve panel shares the timeline’s selected track and respects **nested sub-timeline** mapping when enabled; the **graph** between keys is drawn to match the runtime (including **hold** steps and **float Bezier** where enabled).
- **Graph editor** (dock): **driver → driven** links (scale/offset) with **Bake at playhead**; the selected link shows a **readout** of the **driver** value and the **scaled+offset** value the bake will **write to the driven** track at the current time.
- **Particle editor** (dock): CPU **billboard** preview; optional **multi-stop** **color over life** (2–6 RGBA stops along normalized lifetime, with **Sort stops**). **Ribbon trails** (optional) draws a short **line-strip history** per particle in the unified viewport (CPU preview only, not the Parallax sim). **Write emitter to scene** still maps the **first** and **last** stops to Parallax `ColorStart` / `ColorEnd` (the extra stops are for **SMM preview**). Legacy **Color start** / **Color end** apply when the multi-stop mode is off. When the **Particles** panel has focus, **Edit → Undo / Redo** (and **Ctrl+Z** / **Ctrl+Y** / **Ctrl+Shift+Z**) apply to the **particle editor state** instead of the Parallax scene snapshot; scene undo still applies when another panel is focused.

SMM panel migration follows the same utility rule: use `LibUI::Widgets` primitives and LibUI tool elements for app-level forms/menus first, and reserve raw `ImGui::*` calls for custom rendering/editor canvases that currently require direct draw-list access.

## 2D motion graphics (MG) in SMM

Parallax’s MG authoring supports both:
- **screen-space 2D** (`MGProjectionMode=0`, legacy behavior), and
- **world-space unified sprites** (`MGProjectionMode=1`, MovieMaker unified viewport workflow).

SMM is a **front end** to that data; **attribute names**, **`EvaluateMG`** rules, per-path rendering caveats, and file-level APIs live in **[MotionGraphics.md](MotionGraphics.md)**—this section is the **MovieMaker–specific** map.

**Where you work**
- **Timeline + Curve editor:** Add or select **MG** rows and keyframe properties by name (e.g. `Position`, `Size`, `Depth`, `RotationZ`, root `GradeExposure`, …). MG tracks use the same curve tools and **INI presets** as 3D ([Presets.md](Presets.md)).
- **Properties** (with an **MG** element or root selected): **2D comp**—**nominal** width/height in pixels for **nudge**, **align**, and **snap** (authoring only; it does not set render resolution). For **sprites**: **link W/H**, transform and **sprite FX** (smear, UV scroll/wave, AO, **chroma key**, **rotoscope**, **zoom / radial blur**). For **`MotionGraphicsRootElement`**: **CompositeAlpha**, **MG time scale** (linear speed for all MG sampling), **screen shake**, **chromatic aberration**, and **color grade** (see [MotionGraphics.md](MotionGraphics.md) for semantics).
- **View:** **Unified** viewport composites a **CPU-rasterized** MG image over 3D with the **MG overlay** slider. A dedicated **2D MG** view/panel may be available for isolating the flat layer. **File → Export →** video (see **Saving and export**): resolution **presets** (16:9, 9:16 vertical, 1:1, etc.) and optional **nominal comp** size match the 2D tools.

**What to expect in preview vs ffmpeg export**
- **In-app MG preview** (static 2D viewport / overlay) follows the **CPU raster** path: full per-sprite and full-frame post where implemented.
- **Video export** draws MG through **ImGui + OpenGL**, then applies **color grade + chromatic aberration** on the readback so the encoded file gets matching **global** post. **Per-sprite** effects that are CPU-only in raster may not match the ImGui path pixel-for-pixel—[MotionGraphics.md](MotionGraphics.md) states which path does what.

**Prefabs and elements:** Use **Authoring → Prefabs** to instantiate `MGTextElement`, `MGSpriteElement`, or `MotionGraphicsRootElement` (see prefab `Schema=…` in the TSV). **Depth** on root MG entries controls 2D draw order in raster and export (see **MG depth** under **Unified viewport** above).

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
- **PARALLAX** export writes `.prlx` through `LibParallax::SaveScene`, creates parent folders, updates recent paths, and supports optional **ZSTD** compression (uncompressed header, compressed tail—see format notes in utilities docs). Current exports package resolver-backed media directly into the `.prlx` asset payload section (mesh/audio/texture and other referenced `AssetHash` bytes).
- **File → Export…** opens the export window directly (`.prlx` path, compression, video section).
- **Video export** uses the **ffmpeg CLI** (MP4/MOV, dimensions, FPS, tick range). The export window includes **presets** for common **16:9**, **9:16 (vertical)**, **1:1**, **4:3**, a **2.39:1** frame, and **nominal 2D comp** size (same numbers as the MG 2D Properties tools). You can still type any **Width** / **Height** in range. ffmpeg is optional at build time; the UI indicates whether encoding is available. Export and the **render queue** wrap failures in **try/catch** so a bad encode should not terminate the app; on failure the log shows a **detailed report** (time, paths, resolution, ticks). Use **Copy video export log** or **Copy failure report (full detail)** to put text on the clipboard for bug reports.
- **Render queue:** in **Export**, configure a video job (output path, resolution, fps, tick range, container), then **Add current settings to render queue**. You can queue several jobs (e.g. alternate paths or ranges) and **Run render queue (sequential)** to encode them one after another without re-entering settings.

## Export performance, KPIs, and regression guardrails

Video export is built on a staged producer/consumer pipeline (`evaluate -> capture -> raster -> composite -> post -> encode`) with a decoupled async ffmpeg writer thread. Persistent per-export buffers and a bounded `FrameArena` for transient scratch keep allocator pressure off the hot path. Each frame records per-stage microsecond timings into `IncrementalVideoExportSession::Stats`; on successful completion the export emits a single one-line summary to **DiagLog** (`[VideoExport] done frames=… total_ms=… eval_ms=… cap_ms=… mg_ms=… comp_ms=… post_ms=… write_ms=… wait_ms=… enc_bytes=… max_q=…`).

**Tracked KPIs** (from the DiagLog summary):
- `total_ms / frames` — end-to-end wallclock per frame (the ratio is the effective export FPS).
- `eval_ms`, `cap_ms`, `mg_ms`, `comp_ms`, `post_ms`, `write_ms` — time spent in each stage, summed across the export.
- `wait_ms` — encoder backpressure (time the producer waited for the writer queue or for `fwrite`).
- `max_q` — peak depth of the bounded SPSC queue (`<= 8`); a value pinned at the cap signals encoder backpressure, while `0–1` indicates render-bound.
- `enc_bytes` — total bytes written to the ffmpeg pipe (sanity check against expected `frames * width * height * 4`).

**Recommended benchmark scenarios** (run each at the listed profiles and record the KPIs):
- *Long talking-head* — 5 minutes of one actor + lipsync, light MG. Stresses evaluation hot path and audio mixdown.
- *Dense scene* — `> 8` actors, many lights, multiple `MGSpriteElement`s with FX. Stresses capture and composite.
- *High-motion* — fast camera moves with particle ribbons + chromatic aberration + grade. Stresses post and write stages.
- *Heavy MG* — full-frame motion graphics with smear/UV scroll + multi-stop particle color. Stresses raster + composite.

**Target profiles**: `1080p60`, `1440p60`, `4K30`, `4K60`. `8K30` is best-effort (high-end rigs only).

**Regression guardrails**:
- `tests/SmmExportPipelineTest.cxx` (ctest label `quick`) validates the underlying primitives (lock-free SPSC queue, frame arena, SIMD blend equivalence, tile-equality SIMD/scalar fallback) and asserts a generous wallclock ceiling on a single 1080p `BlendOverRGBA` and `TileEqual` pass so a regression in the SIMD paths is caught even on shared CI runners. The test prints the active SIMD feature set (`scalar`, `sse2`, or `avx2`) for diagnostic context.
- The `[VideoExport] start ... simd=…` line is emitted when an export begins so per-run SIMD provenance is traceable in DiagLog.
- For release branches, run a representative scene at each target profile and compare `total_ms / frames` and `max_q` against the previous baseline; large `wait_ms` increases without an `enc_bytes` change indicate encoder regressions, while `eval_ms` regressions indicate scene/evaluation hot-path regressions.

## Frame cache (LZX-compressed, mmap-backed)

When `VideoExportParams::enableFrameCache` is set, the export pipeline keeps a persistent disk cache of rendered frames keyed by content. On a subsequent export of the same scene/range/profile, the renderer is **bypassed** for any cached frame and the bytes are decompressed straight into the encoder buffer.

**Storage layout** (under `frameCachePath`, default `<temp>/Solstice/SmmFrameCache/<projectFingerprintHex>/`):
- `index.bin` — manifest with a stable header and a flat array of fixed-size entry records (per-key blob offset, sizes, content checksum, created/last-access epochs, access count, delta flag, base key).
- `blob.dat` — append-only compressed payloads. Read paths use `Core::MmapFile` for zero-copy where possible; writes fall back to a normal file append.

**Key derivation:** `BuildFrameKey(projectFingerprint, tick, width, height, fps, postProcessFingerprint)`. The renderer is a pure function of the scene state at `tick` plus the export resolution/fps, so the same key implies a bit-for-bit reusable result. Pass `VideoExportParams::projectFingerprint` for cache hits across runs (e.g. an FNV-1a hash of the `.prlx` bytes); when zero, only the in-session cache helps.

**Compression:** Solstice's in-house `LZX` block compressor (`source/Core/System/LZX.cxx`) is used for all payloads. Random/noisy frames stay close to source size; static or near-static frames compress dramatically. Both the full-frame and the **delta** path use the same compressor.

**Delta frames:** when inserting frame N, the cache attempts an XOR encoding against frame N-1's full payload. If the LZX-compressed delta beats the LZX-compressed full frame by at least `DeltaWinThreshold` (default 5 %), the entry is stored as a delta with `BaseKeyHash` pointing at frame N-1. On lookup the base is decompressed first, then XORed with the delta — both are content-checksummed (FNV-1a 64) so a bit-flip on disk surfaces as a miss rather than a corrupt frame. Delta resolution is bounded to **one level**: if the base is itself a delta, the entry is rejected at insert time so lookups never traverse a chain.

**TTL + eviction:** `TtlSeconds` (default 7 days) drops stale entries on `Open()` and on every `Prune()`. When inserts would push the **live** compressed-byte total over `BudgetBytes` (default 4 GiB), the cache evicts entries by **weighted LRU/LFU score**:

```
score = WLru * recency + WLfu * log2(1 + AccessCount)
```

where `recency ∈ [0, 1]` (1 = just touched, 0 = at TTL or 1 day if TTL=0). Defaults bias toward recency (`WLru=0.6`, `WLfu=0.4`) so re-exporting the most recent timeline keeps its frames warm while a long-cold sweep is the first to go. Deltas score 15 % below their numeric weight so a base-frame that's about to be evicted takes its dependents with it (avoiding orphaned deltas).

**Diagnostics:** the existing `[VideoExport] done …` DiagLog summary is extended with cache counters when the cache is on:

```
cache_lookups=N cache_hits=H(D delta) cache_inserts=I(D delta) cache_evicted=E
cache_bytes_stored=B cache_bytes_saved_delta=S
```

`cache_bytes_saved_delta` is the cumulative `(full_compressed - delta_compressed)` for entries that took the delta path — useful for spotting when delta encoding is paying for itself.

**When the cache is not used:** insert is skipped while capture is in warmup or has been disabled by repeated capture failures, so a degraded run cannot poison the cache for a future clean export.

**Maintenance:** `FrameCache::Compact()` rewrites the blob with only live entries (relocating offsets atomically via tmp + rename) and is the only operation that reclaims on-disk bytes left behind by index-level eviction; useful after long sessions with many evictions. The cache is fully released on session destruction (`unique_ptr` in `IncrementalVideoExportSession`), and `Flush()` is called on the export's done summary so a hard shutdown afterwards loses at most the in-flight insert.

**Validation:** `tests/SmmFrameCacheTest.cxx` (ctest label `quick`) covers round-trip, delta, persistence-across-open, TTL eviction, budget eviction, corrupt-index recovery, and post-compaction lookups.

## Autosave recovery

- While the Parallax scene is **dirty**, MovieMaker periodically writes a **recovery snapshot** of the current `.prlx` bytes (with the same ZSTD option as normal save) into the editor **FileRecovery** store under a per-app key (`smm`). The **Export** window exposes a **Recovery interval (sec)** slider (also persisted in `.smm.json` as `recoveryIntervalSec`).
- **File → Write recovery snapshot now** forces an immediate recovery write (useful before risky experiments).
- On launch, if recovery files exist, the existing **restore from recovery** flow (when shown) can load those bytes into the session. Recovery is **not** a substitute for **Save Project** / **PARALLAX export**; it is a safety net against crashes.

## Assets and media

- Generic asset import uses the **dev-session asset resolver** (in-memory **hash → bytes** for the current session).
- **Raster images** can be imported for **MG sprites** (`MGSpriteElement` **Texture** attribute); common formats include PNG, JPEG, BMP, TGA, WebP, and others supported by the shared image decode path. **`Depth`** on sprites/text controls **2D stacking** within the MG layer (see **MG depth** above).
- **Audio** can be imported for **`AudioSourceElement`** rows (session-backed **AudioAsset**). In **Properties** (with an `AudioSourceElement` selected), you see the **`AudioAsset` hash** when assigned, a short **runtime** note, and **Volume** / **Pitch** with **undo** (scene snapshot) on change. The **waveform** strip supports **click to seek** and **drag to scrub** with **real-time** preview audio; while you **drag**, the **vertical playhead** tracks the **cursor** so scrubbing stays visually locked to the pointer. Use **File** or **Assets → Import audio** to bind bytes; export embeds resolver-backed **AudioAsset** payload bytes in `.prlx`.
- **Video import:** **File → Import video to session…** reads the chosen file into the **dev-session asset resolver** (hash + bytes), the same way other media imports work. SMM does not embed a full clip editor; the import is for **reference**, compositing hooks, or tooling that resolves **session** assets. Decoding/trimming is expected to be handled by external **ffmpeg** workflows or future Parallax features.
- **glTF / glb** follows a **selected-Actor** workflow: add or select an **`ActorElement`**, then import to assign **`MeshAsset`**; export writes the current mesh asset back to `.gltf` / `.glb` when the bytes are in the resolver session.

## Prefabs, asset database, morpheme & lipsync placeholders

- **Prefabs:** define an **id**, **display name**, **Parallax schema** (`CameraElement`, `ActorElement`, `MGTextElement`, …), and **`Key=value` pairs** separated by semicolons (see **Properties** / Parallax attribute names). **Instantiate** adds the element or MG node to the **current** scene (with undo).
- **Simplified asset database:** rows mirror **session** imports (hash + label). Mark **proxy** when a row is a low-LOD or stand-in asset; set **resolves to** (hex hash) when you know the **full-quality** asset hash for reference (runtime swap is still an engine concern; SMM stores **metadata** only).
- **Morpheme-like tokens:** **Extract** in Authoring uses **word-shaped** runs (letters, digits, hyphen) as coarse “morpheme” **proxies**—in the spirit of **2002–03** game tooling (not a real morphological analyser). The **Authoring** tab shows **stats** (word count, unique tokens, character count, **vowel density**) to sanity-check the line.
- **Lipsync:** implemented in **`SmmLipsyncMorpheme`** + Authoring UI. **Mode 0** = legacy **even word windows** along `[StartTick, EndTick]`; **Mode 1** = **vowel-centered** sub-timing (MG **Depth** placeholder). **Mode 2** = **English letter → viseme** samples (OVR-style ids: `aa`, `PP`, `sil`, …) written to the selected **`ActorElement`** as Parallax channels **`FacialVisemeId`** (`String`) and **`FacialVisemeWeight`** (`Float`)—these drive **`EvaluateScene`** → **`ActorFacialPoses`** (Arzachel facial stack) for runtime/preview. **Apply to MGText** uses modes 0–1 only; **Apply viseme keys to selected Actor** uses the same stub span/text. **TSV** lines: `lipsync`, `label`, `startTick`, `endTick`, `text`, then optional `strength`, **`phoneticMode` (0, 1, or 2)**, `maxKeyframes`. **Railguards:** same caps/notes as before (text trim, max keyframes, **Start=End** single-sample). Stubs persist in the authoring TSV without bumping `.smm.json` **version**.

## Collaboration and long-form animation

- **Shared `presets/` tree:** Commit `presets/Keyframe/` and `presets/Timeline/` next to the **project** `.smm.json` (or rely on shipped defaults under the executable) so animators share the same **curve** and **shot-range** INI without touching Parallax files. Optional `Description=`, `Author=`, and `Tags=` in those INIs are surfaced in the **Curve** panel and the **shot / act** row for quick handoff notes.
- **Reload without restart:** The curve panel’s **Reload INI** rescans from disk; use it after a teammate updates a sidecar or you merge Git changes to `presets/`.
- **Nested sub-timeline + INI shot ranges:** For long scenes, set **Nested sub-range** to zoom the timeline and curve editor to a working window; `TimelineRangePreset` INIs (see [Presets.md](Presets.md)) assign consistent `[start, end)` ticks for acts or review beats, with **Apply to nested sub-range** and **Jump playhead to range start** so moving between “acts” is one click. Subfolders under `presets/Keyframe/` are supported so teams can group presets by show or act without filename clashes (ids still override by **last loaded** for duplicates).

## API alignment

`SDK/SolsticeAPI/V1/Smm.h` provides a dedicated SMM API lane for:

- session state / playhead clamp (`SolsticeV1_SmmSessionState`, `SolsticeV1_SmmClampPlayhead`)
- viewer tab policy (`SolsticeV1_SmmViewerTab`, `SolsticeV1_SmmSetViewerTab`)
- export gating policy helpers for integrations (`SolsticeV1_SmmExportIntent`, `SolsticeV1_SmmCanRunExport`)

For **2D MG** workflow in the MovieMaker UI, see **[2D motion graphics (MG) in SMM](#2d-motion-graphics-mg-in-smm)** above. For Parallax **schema / attributes**, **`EvaluateMG`**, raster vs compositor vs ffmpeg, see **[MotionGraphics.md](MotionGraphics.md)**. General tooling and packaging: **[Utilities.md](Utilities.md)** (SMM section), **[Packaging.md](Packaging.md)**, and [`MovieMaker.fst`](../tools/packaging/MovieMaker.fst) (optional **ffmpeg** beside the executable when configured).
