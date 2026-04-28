#pragma once

namespace Solstice::WarsOfHeaven {

/// After `Render::SoftwareRenderer` draws the 3D scene, call this (same frame) after `ImGui` `NewFrame`
/// and before any `ImGui::Begin` that should appear above the field. Fills the viewport with a deep-space
/// gradient, distant multicolored points, and soft “glow” passes for a glass-friendly backdrop.
void DrawWohStarryMenuBackground();

} // namespace Solstice::WarsOfHeaven
