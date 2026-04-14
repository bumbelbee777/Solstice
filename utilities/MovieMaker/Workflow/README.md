# MovieMaker `Workflow/` — editor routines

Code-first helpers for Solstice Movie Maker (SMM): timeline/playhead utilities and Parallax scene edits. UI in `Main.cxx` should call into this namespace instead of duplicating logic.

- **Timeline**: `ClampPlayhead`, jump start/end, snap to whole seconds (tick space).
- **Scene**: `ShiftSceneKeyframes` wraps `Parallax::ShiftAllKeyframeTimes`.

Extend with MG-specific wizards here as APIs stabilize.
