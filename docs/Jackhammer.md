# Jackhammer (LevelEditor) — feature reference

Jackhammer is the Solstice **`.smf`** authoring tool (CMake target **`LevelEditor`**). This document complements the overview in [Utilities.md](Utilities.md) and records **every major authoring surface** as of Technology Preview 1.

## Source layout (`utilities/LevelEditor/`)

| File | Role |
| --- | --- |
| [Main.cxx](../utilities/LevelEditor/Main.cxx) | `main()`: optional Win32 crash diagnostics, calls `Jackhammer::RunApp`. |
| [JackhammerApp.cxx](../utilities/LevelEditor/JackhammerApp.cxx) | `RunApp`: SDL/GL, ImGui, map load/save, most docked panels, engine viewport, undo. |
| [JackhammerApp.hxx](../utilities/LevelEditor/JackhammerApp.hxx) | `Jackhammer::RunApp` declaration. |
| [JackhammerSpatial.cxx](../utilities/LevelEditor/JackhammerSpatial.cxx) | BSP / AABB / box-brush math, **axis-aligned CSG** helpers (union / subtract / **XOR** pieces). |
| [JackhammerViewportDraw.cxx](../utilities/LevelEditor/JackhammerViewportDraw.cxx) | BSP / octree / light / particle overlays. |
| [JackhammerMeshOps.cxx](../utilities/LevelEditor/JackhammerMeshOps.cxx) | Mesh workshop: primitives, **heightfield grid**, sew/weld, **AABB of mesh**, **snap verts to grid**. |
| [JackhammerViewportGeoTools.cxx](../utilities/LevelEditor/JackhammerViewportGeoTools.cxx) | **Measure** and **terrain sculpt** input on the engine viewport. |
| [JackhammerPrefabs.cxx](../utilities/LevelEditor/JackhammerPrefabs.cxx) | **Prefab** I/O: minimal `.smf` save, append-entities with offset. |
| [JackhammerBspTextureOps.cxx](../utilities/LevelEditor/JackhammerBspTextureOps.cxx) | UV alignment helpers (world / view / default face). |
| [JackhammerLightmapBake.cxx](../utilities/LevelEditor/JackhammerLightmapBake.cxx) | Simple N·L **lightmap** bake to PNG. |
| [JackhammerTexturePaint.cxx](../utilities/LevelEditor/JackhammerTexturePaint.cxx) | 2D face paint. |
| [JackhammerArzachelProps.cxx](../utilities/LevelEditor/JackhammerArzachelProps.cxx) | Procedural Arzachel props into mesh buffer. |
| [JackhammerParticles.hxx](../utilities/LevelEditor/JackhammerParticles.hxx) | Particle-emitter class name / helpers. |

Larger UI remains in **`JackhammerApp.cxx`** by design; new tools are factored out when they need testing or shared math.

## Map & I/O

- **New / open / save** `.smf` via LibSmf; optional **ZSTD** tail; **file watch** and **settings** (like other utilities).
- **Validate** (F7) and **apply gameplay** (F8) match [Utilities.md](Utilities.md) and **SolsticeAPI** `Smf` exports when the engine DLL is present.
- **Undo/Redo** uses full-map `LibUI::Undo::SnapshotStack<SmfMap>`.

## Engine viewport (general)

- **Orbit** camera; **Top / Front / Side / Persp**; **Focus**; **place snap**; **grid**; **Ctrl+LMB** (when **no** geometry tool) places selected entity on **Y = current origin** at XZ hit.
- **Orbit** uses **LMB** by default. **Block**, **Measure**, and **Terrain** tools rebind **orbit to RMB** so **LMB** is free for the tool.
- **Arrow keys** nudge the selected **entity** origin on XZ when the viewport is hovered (uses place snap or grid).
- **Overlays:** BSP, octree, lights, particles, “all” entity markers, selection highlight.

## Geometry tools (Spatial → BSP → “Geometry tools”)

Combo **Active tool:**

1. **None** — default; **Ctrl+LMB** = place selected entity; **LMB** = orbit.
2. **Block** — **LMB drag** on the **XZ plane** at **Block base Y** draws a **box**; if a BSP exists, sets the **selected node’s slab**; if not, **fills the mesh workshop** with that AABB.
3. **Measure** — successive **LMB** clicks on the XZ plane at **Block base Y**: first **A**, second **B**; third click **moves A** and **clears B**. Shows **screen-space** line + **world distance** and **ΔX/ΔY/ΔZ** when both points exist. **Reset** in the panel clears A/B. Snapping uses **place snap** when set.
4. **Terrain sculpt** — **LMB+drag** on XZ: within **Brush radius (world)**, all **mesh workshop** vertices with XZ inside the circle get **Y += Raise / frame** (per frame while held). **Negative** “raise” lowers. Normals are recalculated. **Requires** a heightfield (or any mesh) in the workshop. **RMB** = orbit.

**Block base Y** is shared as the **horizontal plane** for block, measure, and terrain (Hammer-style ground reference).

## Hammer-style **terrain** (scaffold)

Terrain is not a separate SMF “terrain” object; it is the **mesh workshop** pipeline:

1. Under **Parametric mesh primitives**, use **Heightfield (Hammer-style terrain scaffold)** for arbitrary **cell** counts, or **Displacement patch** for Hammer-style **2^p** segments per side (vertices `(2^p+1)²`, **power** 1‥4).
2. Set **origin XZ**, **size XZ**, **base Y** (and cell counts or **power**) → **Build** → **workshop** (regular grid, **Y-up**).
3. Switch **Active tool** to **Terrain sculpt** to raise/lower, or use **sub-object** vertex edits / Laplacian smooth / weld.

**Export** to the rest of the map is not automatic: the workshop is in-memory; use your own export path to glTF or a future map hook.

## Vertex **snapping** (mesh workshop)

- **Vertex snap grid (0=off):** when the value is **positive**, **Apply to vertex** snaps the entered position; **Snap all mesh vertices to grid** rounds every vertex in the scratch mesh. Works with **Weld** for vertex-merge-style cleanup.

## Prefabs (SMM-style **instancing** of entities)

Parallax/SMM use **element** instancing; Jackhammer uses **SMF entities**:

- **Save selected entity to prefab** writes a **minimal** `.smf` containing **only** that entity (no BSP, lights, or path table).
- **Append prefab** loads a `.smf` and **appends all its entities** to the current map, adding **`origin` / `position` `Vec3`** by **Append offset** and **renaming** entities to stay unique.
- **Set offset from measure point A** copies the first **measure** point (if set) into the offset vector for quick **click-to-place** alignment.

A prefab with **one** entity behaves like a **single** instance; a prefab saved from a file with many entities is a **group** instance. Multi-select for saving several entities in one go is a possible future addition.

## Box-brush **CSG** (axis-aligned, **arbitrary in count**, not arbitrary mesh)

All slab CSG here operates on **axis-aligned bounding boxes** of BSP **slab** volumes:

- **Union (hull)** — single AABB.
- **Union (exact)**, **Subtract (all)**, **Symmetric diff (XOR)** — piece lists (up to **6** for a single box minus box, or **up to 12** for XOR) stored in a **buffer**; **Apply** pastes a piece onto the **selected** node’s slab. **Box brush** trees are re-synced when the map uses the canonical 6-plane brush.
- **Mesh workshop AABB → buffer** — computes the **tight AABB** of the scratch **triangle mesh** and stores it as **one** buffer piece (a **stand-in** when you need “brush” from edited geometry; true **mesh** CSG is **not** implemented).

**Non-axis-aligned** brushes or **triangle-level** boolean are **out of scope** for this path; use external DCC and import, or the mesh workshop for authoring only.

## BSP texture / **lightmap** / paint

- See **Texture alignment** (fit, center, rotate, **lock**, world / view / default align, **uniform scale**).
- **Baked lightmap** (BKM1): per-face **PNG** in world **u×v** on the selected BSP slab. Options include **linear ambient** (add before tonemap), **front (+N) vs back (−N)** lighting, and **spot cone** shaping (inner/outer degrees) when “model spot lights” is enabled. See [Utilities.md](Utilities.md) and `JackhammerLightmapBake.hxx`.
- **Face texture paint (2D)** and material paths are documented in the same overview.

## Keyboard / workflow summary

| Action | Default |
| --- | --- |
| Validate map | F7 |
| Apply gameplay (DLL) | F8 |
| Place entity (XZ) | Ctrl + LMB (only when **Active tool = None**) |
| Orbit (when tool uses LMB) | RMB |
| Nudge selected origin | Arrows (viewport focused) |
| Measure | **Measure** tool, LMB on ground |
| Terrain | **Terrain sculpt** + workshop heightfield |
| Entity prefab | **Prefabs** under Spatial → BSP tab |

## Related

- [Utilities.md](Utilities.md) — all three tools, LibUI, build flags.
- [Packaging.md](Packaging.md) — FST + [`LevelEditor.fst`](../tools/packaging/LevelEditor.fst) for shipping the editor.
- [Smf.h](../SDK/SolsticeAPI/V1/Smf.h) — engine-side binary validate when linked.
- [example/JhSmfProject/README.md](../example/JhSmfProject/README.md) — demo project layout.
