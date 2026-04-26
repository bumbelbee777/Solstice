# Jackhammer demo project (`JhSmfProject`)

Small **Solstice Map Format** workspace for **[Jackhammer](../../docs/Utilities.md#jackhammer-leveleditor-smf)** (LevelEditor): two mesh props, **SMAL** authoring lights, **acoustic zones** with bundled **WAV** cues, a **BSP** floor slab with a checker **PPM** texture, a root **octree** cell, path-table rows, and a **`demo_ceramic.smat`** for material preview.

## Files

| Path | Purpose |
| --- | --- |
| `JackhammerDemo.smf` | Main map (open this in Jackhammer). |
| `teapot.gltf` | Legacy glTF mesh used by **`teapot_west`** / **`teapot_east`** (`modelPath`). |
| `assets/audio/demo_hall.wav` | Low tone — referenced as **ambience** on **`hall_main`**. |
| `assets/audio/demo_room.wav` | Higher tone — referenced as **music** on **`alcove`**. |
| `assets/textures/checker_8.ppm` | 8×8 checker; BSP face texture + entity maps. |
| `assets/materials/demo_ceramic.smat` | Solstice Material v1 — **`materialPath`** on **`teapot_west`**. |

## Try in Jackhammer

1. Build with **`SOLSTICE_BUILD_UTILITIES`** ON; run **`LevelEditor`** from the CMake **`bin`** output (so **`SolsticeEngine.dll`** is beside the exe for viewport capture).
2. **File → Open** → `JackhammerDemo.smf` (keep working directory or paths relative to this folder).
3. **Validate map** (**F7**); **Apply gameplay** (**F8**) to push zones/lights into the engine path used by the DLL.
4. Explore **Gameplay (acoustic zones & lights)**, **Spatial (BSP / Octree)**, entity **Material (.smat)** / **Diffuse texture**, and the viewport **Material preview** toolbar (`.smat` + optional maps).
5. Use **Import…** on a zone to copy another audio file into **`assets/audio/`** (map must be saved first so the tool knows the `.smf` directory).

## Regenerating the map and assets

The repo includes generated binaries so you do not need to run the tool to try the demo. To rebuild **`JackhammerDemo.smf`** and overwrite the **`assets/`** files (not `teapot.gltf`):

```bash
cmake -S . -B build -DSOLSTICE_BUILD_JH_SMF_DEMO_GEN=ON
cmake --build build --target JhSmfProjectGen
./build/bin/Debug/JhSmfProjectGen   # pass absolute path to this folder if not using default example/JhSmfProject from cwd
```

On Windows, the executable is `build\bin\Debug\JhSmfProjectGen.exe`.

## Notes

- **SMF skybox:** When the engine runs **`SolsticeV1_SmfApplyGameplay`** / **`MapSerializer::ApplyGameplayFromSmfMap`**, optional **`SmfSkybox`** is published to [`Core::AuthoringSkyboxBus`](../../source/Core/AuthoringSkyboxBus.hxx). Games call [`ApplyAuthoringSkyboxBusToSkybox`](../../source/Render/Scene/AuthoringSkyboxApply.hxx) each frame (see Blizzard / Hyperbourne) to refresh the bgfx cubemap when face paths change.
- **`proc_marker`** is a text-only **Prop** pointing to **[Arzachel](../../docs/Arzachel.md)** for procedural mesh authoring; SMF entities here focus on glTF + editor spatial/gameplay features.
- **`teapot.gltf`** is an older glTF style; some runtimes expect glTF 2.0. It remains useful for path-table, selection, and transform practice in Jackhammer.
