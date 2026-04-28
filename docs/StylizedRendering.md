# Stylized rendering & screen effects

This page describes **cel shading**, **rim lighting**, **vertex wobble**, **screen-space shockwave**, **height + distance fog**, and how to combine **2D sprites** with **bone transforms**. The main render path is BGFX: `vs_standard` / `fs_standard` for the scene pass and `fs_post` for HDR post ([Renderer.md](Renderer.md)).

## Per-material stylized controls (`MaterialExtras`)

When a `Core::Material` has a non-null `Extras` pointer, these fields feed the `u_stylize` uniform (see `source/Shaders/vs_standard.sc` and `fs_standard.sc`):

| Field | Role |
|--------|------|
| `CelBands` | `0` = normal PBR key-light response. **`2`–`8`** = quantize the sun lambert term into that many bands (toon/cel). Point lights are not re-banded in the current shader. |
| `RimOverdrive` | `0` = default rim weight. **`> 0`** scales the Fresnel-style rim (values around **2–4** read as “hot” edges). |
| `VertexWobbleAmplitude` | World-space displacement along **vertex normals** using a cheap sin (good for subtle cloth/water/muscle). Works on any mesh submitted with normals, including **CPU-skinned** meshes (final positions are already deformed when they hit the GPU). |
| `VertexWobblePhase` | Add to the sin phase; animate this each frame for motion. |

JSON material serialization includes these keys under `Extras` (see `Material.cxx`).

## Screen-space shockwave

`PostProcessing::SetShockwaveSettings` sets `u_shockwaveParams`: **xy** = center in UV (0–1), **z** = ring radius in aspect-corrected UV space, **w** = strength (**0** disables). The post shader refracts **color** samples (not depth) to keep TAA and depth-based effects stable.

Typical workflow: on explosion or impact, bump `Strength` for a few frames, animate `RingRadius` outward, then set `Strength` back to `0`.

## Volumetric-style screen fog

Full **ray-marched volumes** are not in this path; you have two complementary options:

1. **God rays** — existing `SetVolumetricTexture` + `SetGodRaySettings` (CPU/light texture into `fs_post`).
2. **Screen fog** — `PostProcessing::SetScreenFogSettings` (`ScreenFogSettings`): exponential fog in **camera distance** plus optional **height fog** using world Y (`HeightAnchorY` / `HeightFalloff`). Applied in **linear HDR** before ACES. Enable with `Enabled = true` and tune `DistanceDensity`, `Mix`, and `FogR/G/B`.
3. **Fog dither (noise)** — `PostProcessing::CinematicViewState::ScreenFogDither` (and `u_FogDither` in `fs_post`) adds a small per-pixel **hash** to the fog mix so large uniform fog does not band; **0** = off.

**3D depth-scaled chromatic aberration** and **smear frames** (extra TAA history blend) are exposed the same way on `PostProcessing::CinematicViewState` (`u_ChromaticParams`, `u_SmearFrame`): radial R/B offset from a tunable **center** with strength **scaled by linear depth** for a “lens at infinity” look, and **smear** toward `s_texHistory` when TAA is valid. MovieMaker’s **SMM** authoring TSV and **Authoring → 3D schematic post** drive these for **EditorEnginePreview** orbit capture.

Use (1) for light shafts and (2) for atmospheric depth and ground mist; use (3) and the cinematic fields when you need stable fog in gradients or stylized 3D post in tool previews.

## 2D sprites on 3D bones (integration pattern)

There is no single “sprite socket” draw call in the core scene pass today. The intended pattern is:

1. Evaluate your animation (e.g. `Skeleton` / `Pose` or Parallax rig) to get a **world matrix** for the bone you care about.
2. Create or update a **scene object** that carries a **billboard** or **card** mesh (the engine ships `vs_billboard` / `fs_billboard` for camera-facing quads) and set its transform each frame to **position** (and optionally recompute from the bone matrix + camera for billboarding).
3. Use a **transparent** or **cutout** material; depth order with other meshes is the same as any other transparent object.

Authoring tools can automate (2) by parenting a “sprite” object to a bone name in the DCC export and writing the bone index at load time.

## Related

- [Renderer.md](Renderer.md) — pipeline, `PostProcessing`, view IDs.
- [MotionGraphics.md](MotionGraphics.md) — 2D comp / MG (orthogonal to 3D bone cards, but can be composited in MovieMaker).
