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
