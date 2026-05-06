# Facial animation & lipsync — eval vs runtime parity (TP1)

Short matrix for **data-first** facial work: what is evaluated in Parallax vs what shipped games still need.

| Source | Output / API | Eval / tools (today) | Game renderer (typical gap) |
| --- | --- | --- | --- |
| Parallax channels `FacialMoodName`, `FacialMoodWeight`, `FacialVisemeId`, `FacialVisemeWeight` | `SceneEvaluationResult::ActorFacialPoses` via `Arzachel::EvaluateFacialAtTimeToMaps` | **Yes** — `ParallaxEvaluate.cxx` | Apply **bone deltas** + **MorphNameHash** weights to skinned meshes / blend shapes |
| SMM `SmmLipsyncMorpheme` modes 0–1 | MGText **Depth** keys | **Authoring** | N/A (2D draw order only) |
| SMM mode 2 | Viseme samples on actor channels | **Yes** — writes Parallax channels | Same as row 1 |
| Arzachel `ExpressionStack`, `VisemeSet::Standard()`, blink / saccade | Named maps in `ActorFacialPose` | **Yes** | Shader / skinning path |
| Preview (`PreviewMouthOpenHint`) | Gizmo scale hint from jaw / morphs | **Yes** | Not full facial solve |

**Doc anchors:** [Arzachel.md](Arzachel.md) § *Facial animation*, [SMM.md](SMM.md) § *Lipsync*, [ParallaxTypes.hxx](../SDK/LibParallax/include/Parallax/ParallaxTypes.hxx) (`ActorFacialPose`, channel name constants).

**Runtime module:** [`ParallaxPlaybackSession`](../SDK/LibParallax/include/Parallax/ParallaxPlayback.hxx) evaluates scenes and publishes sky; games should consume `ActorFacialPoses` the same way SMM’s evaluation does, then drive skeleton/morph targets.

## Supercharged runtime path

Facial/lipsync now stays lightweight but adds higher-quality controls:

- `ExpressionStack` now supports viseme lookahead:
  - `VisemeId`, `VisemeStrength`
  - `NextVisemeId`, `NextVisemeStrength`, `VisemeBlend`
- `FacialSolveConfig` adds composable solve controls:
  - global expression/viseme gain
  - morph soft-clamp and optional normalization
  - viseme coarticulation toggle
- `EvaluateFacialAtTimeToMapsEx(...)` provides the extended solve path while keeping `EvaluateFacialAtTimeToMaps(...)` intact for compatibility.

## Text lipsync uplift

`TextLipSync` now supports richer text-to-viseme generation through `LipSyncTextConfig` and
`BuildVisemeKeyframesFromEnglishTextEx(...)`:

- digraph-aware mapping (`th`, `ch`, `sh`, `ph`, `ng`, `qu`, `oo`, `ee`)
- vowel/consonant strength shaping
- punctuation-driven pause visemes
- coarticulation smoothing across neighboring viseme keys

The original `BuildVisemeKeyframesFromEnglishText(...)` remains as a stable default wrapper.

## Broader second pass (transition + timeline solve)

The runtime now supports animation-friendly transition logic without adding heavy graph machinery:

- `ExpressionEnvelope`:
  - attack / hold / release timing
  - shape exponent for smooth vs snappy ramps
- `ExpressionTimelineState`:
  - concurrent current/target expression state
  - age tracking and envelope-weighted blending
- timeline helpers:
  - `EvaluateEnvelopeWeight(...)`
  - `AdvanceExpressionTimeline(...)`
  - `BuildStackFromTimeline(...)`
  - `EvaluateFacialFromTimelineToMaps(...)`

This keeps the system self-evident and composable: gameplay/AI can choose expressions, timeline utilities handle weight evolution, and the existing map-based solver remains the final integration point.

## Viseme timeline utilities

For deterministic lipsync playback and runtime streaming:

- `SampleVisemeAtTick(...)` gives current viseme, next viseme, blend, and strength at a timeline tick.
- `CompactVisemeKeyframes(...)` removes near-redundant triplets to reduce channel density while preserving shape.

## ECS facial state machine

Solstice now includes a lightweight intent-driven ECS path:

- component: `ECS::FacialStateMachine` in `source/Entity/Components/FacialComponents.hxx`
- system: `Game::FacialSystem` in `source/Game/Systems/FacialSystem.*`
- registered in both `FPSGame` and `ThirdPersonGame` simulation phases

Intent states:

- `Idle`
- `Listen`
- `Speak`
- `Emote`

The system handles:

- expression target selection from mood/emote intent
- envelope/timeline progression (`ExpressionTimelineState`)
- dialogue text -> viseme key generation
- viseme sampling with lookahead blend
- final map solve into `OutputBoneDeltas` + `OutputMorphs`

Script control helpers (entity-based):

- `Arzachel.FacialState.Set(entityId, state)`
- `Arzachel.FacialState.SetMood(entityId, moodName)`
- `Arzachel.FacialState.SetEmote(entityId, emoteName)`
- `Arzachel.FacialState.SetDialogue(entityId, text, startTick?, endTick?)`
