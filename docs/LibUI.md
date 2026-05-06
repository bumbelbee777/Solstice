# LibUI authoring primitives

`LibUI` is the shared utility UI library used by Jackhammer, MovieMaker (SMM), and Refulgent.
This note captures preferred quick-win primitives for cross-tool consistency.

## Core runtime configuration

Use `LibUI::Core::RuntimeConfig` to override app identity and persistence defaults before `LibUI::Core::Initialize`:

- `AppDisplayName`
- `StateDirectory`
- `ImGuiIniFilename`
- `RecentPathsFilename`
- diagnostic/crash env var names and file names

This removes hard coupling to a single product name while remaining source-compatible with existing defaults.

## Preferred shared primitives

- **Inline banners:** `LibUI::Widgets::DrawInlineAlert(...)`
  - Use for warning/error/info rows with optional dismiss.
- **Virtualized tables:** `LibUI::Widgets::DrawFilterableVirtualTable(...)`
  - Use for large lists with a filter row and `ImGuiListClipper` under the hood.
- **Workspace shells:** `LibUI::Layout::BeginThreePaneWorkspace(...)` and `BeginTwoPaneFixedLeftTable(...)`
  - Use for canonical authoring layouts instead of hand-rolled width math.
- **Path rows with browse buttons:** `LibUI::Tools::InputPathOpenBrowseRowHint(...)` and `InputPathOpenBrowseRowHintString(...)`
  - Use for `InputText + browse` rows to centralize width math, dialog launch, and optional map-relative path remapping.

## File dialog defaults

`LibUI::FileDialogs::ShowOpenFile/ShowSaveFile` now default to `All (*)` when no filter list is supplied.
App-specific formats (for example `.smf`, `.prlx`, `.relic`) should be provided by callers.

## Include style

Use rooted LibUI includes:

- `#include "LibUI/Core/Core.hxx"`
- `#include "LibUI/FileDialogs/FileDialogs.hxx"`
- `#include "LibUI/Icons/Icons.hxx"`

Avoid relative include paths like `../Core/Core.hxx` in public LibUI headers.
