# Narrative, dialogue, and cutscenes

This document describes the Solstice **narrative stack**: data formats (`solstice.narrative.v1`), runtime (`DialogueTree`, `NarrativeRuntime`, `DialoguePresenter`), **cutscenes** (`CutscenePlayer`), Moonwalk bindings, tooling (Sharpon, LevelEditor scaffold), and migration away from hard-coded C++ dialogue.

## Data format: `solstice.narrative.v1`

Primary interchange is **JSON**. A minimal document has:

- `format`: `"solstice.narrative.v1"`
- `startNodeId`: entry node id
- `nodes`: array of node objects
- Optional `provenance`: `{ "source": "json" | "yaml" | ... }` for dual-canonical tooling

Each **node** supports:

| Field | Role |
|--------|------|
| `nodeId` | Stable id (required for serialization) |
| `speakerName`, `text` | Line content |
| `nextNodeId` | Linear successor (empty ends branch) |
| `choices` | `{ "label", "targetNodeId" }[]` for branching |
| `voiceAssetPath` | Path passed to `SFXManager` voice playback |
| `subtitleId` | Stable id for localization tables |
| `subtitleText` | If set, shown instead of `Text` when subtitles are enabled |
| `speakerStyleTag` | Reserved for presenter styles |
| `onEnterEvent` | Reserved for game/script bridge |

**YAML**: A minimal subset is supported for load via `NarrativeDocumentLoadFile` when the extension is `.yaml` / `.yml`. Complex branching is safest in JSON; use **Sharpon** “JSON to YAML” for a readable export.

## Runtime

- **`DialogueTree`**: graph navigation, `Validate()`, `JumpToNodeId()` for scripts/cutscenes.
- **`NarrativeRuntime`**: applies `GameplaySettings` / `AudioSettings` (subtitles on/off, size, voice volume) and plays `voiceAssetPath` lines.
- **`DialoguePresenter`**: typewriter UI; uses `NarrativeRuntime::GetDisplayText()` for localized subtitle text.

Wire-up for scripts:

- Set **`NarrativeBridge`** pointers (`DialogueTree`, `NarrativeRuntime`, `CutscenePlayer`) from game bootstrap (see `VisualNovelGame`).

## Cutscenes

`CutscenePlayer` loads **JSON**:

```json
{
  "id": "intro",
  "durationSeconds": 5.0,
  "events": [
    { "timeSeconds": 0.0, "type": "emit", "payload": "MyEvent" },
    { "timeSeconds": 1.0, "type": "dialogueJump", "payload": "someNodeId" },
    { "timeSeconds": 2.0, "type": "log", "payload": "message" }
  ]
}
```

- **`dialogueJump`**: moves `NarrativeBridge`’s dialogue to `payload` node id and refreshes voiceline via `NarrativeRuntime`.
- **`emit` / `log`**: optional `OnEmit` callback on the player.
- If `durationSeconds` is omitted, it is inferred from the last event time.

## Moonwalk (ScriptManager)

Registered natives include:

| Native | Purpose |
|--------|---------|
| `Dialogue.LoadFromFile(path)` | Load narrative JSON/YAML into the bridge tree |
| `Dialogue.Start` | Start dialogue; first line voiceline |
| `Dialogue.Advance` | Linear advance |
| `Dialogue.AdvanceChoice(i)` | Branch choice index |
| `Dialogue.JumpTo(nodeId)` | Jump to node |
| `Dialogue.CurrentNodeId` | Current id string |
| `Dialogue.IsAtEnd` | 1 if at end |
| `Cutscene.LoadFromFile` | Load cutscene JSON |
| `Cutscene.Play` / `Stop` / `Skip` / `IsPlaying` | Control playback |

(`ScriptManager::Update` must run for cutscene timelines.)

## C API (SolsticeEngine)

- `SolsticeV1_NarrativeValidateJSON` — parse + graph validation; errors as newline-separated text.
- `SolsticeV1_NarrativeJSONToYAML` — export YAML text from JSON.
- `SolsticeV1_CutsceneValidateJSON` — parse cutscene JSON with the same rules as runtime `CutscenePlayer::LoadFromJSONString` (root object, `events` array, optional `id` / `durationSeconds`). See **Cutscenes** above for the JSON shape.

Headers: `SDK/SolsticeAPI/V1/Narrative.h`, `SDK/SolsticeAPI/V1/Cutscene.h` (also included from `SolsticeAPI.h`).

## Tooling

- **Sharpon**: **Tools → Narrative JSON** (or **Narrative** button): edit JSON, **Validate**, **JSON to YAML** using the DLL exports above. **Cutscene JSON** panel: edit timeline JSON, **Validate** via `SolsticeV1_CutsceneValidateJSON`.
- **LevelEditor (Jackhammer)**: edit `.smf` maps; **Validate map** and templates — see [Utilities.md](Utilities.md).

## Migration from hard-coded C++ trees

1. Move inline `AddNode` sequences to `sample_narrative.json` (or your path).
2. Load with `NarrativeDocumentLoadFile` → `ToDialogueTree`, or call `Dialogue.LoadFromFile` from Moonwalk after setting `NarrativeBridge`.
3. Keep a fallback `InitializeDialogueTree()` path for tests without assets (optional).

## Examples

- `example/VisualNovel/assets/sample_narrative.json`
- `example/VisualNovel/assets/sample_cutscene.json`
