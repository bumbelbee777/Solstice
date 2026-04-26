#include "LibUI/Timeline/TimelineWidget.hxx"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace LibUI::Timeline {

namespace {

static uint64_t NestedSpanTicks(const TimelineState& s) {
    if (!s.nestedViewEnabled) {
        return 0;
    }
    return s.nestedRangeEndTick - s.nestedRangeStartTick;
}

static float TickToX(uint64_t tick, const TimelineState& state, float x0, float width) {
    const uint64_t span = NestedSpanTicks(state);
    if (span == 0) {
        const uint64_t duration = std::max<uint64_t>(state.durationTicks, 1);
        const double denom = static_cast<double>(duration);
        const double t = static_cast<double>(std::min(tick, duration)) / denom;
        return x0 + static_cast<float>(t) * width;
    }
    const uint64_t lo = state.nestedRangeStartTick;
    const uint64_t denom = std::max<uint64_t>(span, 1);
    const int64_t ti = static_cast<int64_t>(tick);
    const int64_t l = static_cast<int64_t>(lo);
    const int64_t r = static_cast<int64_t>(lo + span);
    double u = 0.0;
    if (ti <= l) {
        u = 0.0;
    } else if (ti >= r) {
        u = 1.0;
    } else {
        u = static_cast<double>(ti - l) / static_cast<double>(denom);
    }
    return x0 + static_cast<float>(u) * width;
}

static uint64_t XToTick(float x, const TimelineState& state, float x0, float width) {
    const float denom = std::max(1.0f, width);
    const float n = std::clamp((x - x0) / denom, 0.0f, 1.0f);
    const uint64_t span = NestedSpanTicks(state);
    if (span == 0) {
        const uint64_t duration = std::max<uint64_t>(state.durationTicks, 1);
        return static_cast<uint64_t>(n * static_cast<float>(duration));
    }
    const uint64_t lo = state.nestedRangeStartTick;
    const uint64_t denomT = std::max<uint64_t>(span, 1);
    const double t = static_cast<double>(lo) + static_cast<double>(n) * static_cast<double>(denomT);
    uint64_t tick = static_cast<uint64_t>(std::llround(t));
    tick = std::min(tick, state.durationTicks);
    return tick;
}

static void DrawDiamond(ImDrawList* dl, ImVec2 c, float r, ImU32 col, ImU32 outline) {
    const ImVec2 pts[4] = {ImVec2(c.x, c.y - r), ImVec2(c.x + r, c.y), ImVec2(c.x, c.y + r), ImVec2(c.x - r, c.y)};
    dl->AddConvexPolyFilled(pts, 4, col);
    dl->AddPolyline(pts, 4, outline, ImDrawFlags_Closed, 1.0f);
}

} // namespace

bool DrawAnimationTimeline(const char* strId, TimelineState& state, const ImVec2& size) {
    state.durationTicks = std::max<uint64_t>(state.durationTicks, 1);
    state.playheadTick = std::min(state.playheadTick, state.durationTicks);
    TimelineClampNestedRange(state);
    state.zoom = std::clamp(state.zoom, 0.1f, 32.0f);
    state.rowHeight = std::clamp(state.rowHeight, 18.0f, 42.0f);
    state.labelWidth = std::clamp(state.labelWidth, 120.0f, 280.0f);

    const ImGuiStyle& style = ImGui::GetStyle();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const ImVec2 timelineSize(size.x > 0.0f ? size.x : avail.x, size.y > 0.0f ? size.y : 160.0f);
    bool changed = false;

    ImGui::PushID(strId ? strId : "AnimationTimeline");
    ImGui::BeginChild("timeline_canvas", timelineSize, true,
        ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoSavedSettings);
    {
        ImGuiIO& ioT = ImGui::GetIO();
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ioT.KeyCtrl) {
            if (ioT.MouseWheel > 0.f) {
                state.zoom = (std::min)(32.0f, state.zoom * 1.12f);
                changed = true;
            } else if (ioT.MouseWheel < 0.f) {
                state.zoom = (std::max)(0.1f, state.zoom / 1.12f);
                changed = true;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Equal) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd)) {
                state.zoom = (std::min)(32.0f, state.zoom * 1.12f);
                changed = true;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Minus) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract)) {
                state.zoom = (std::max)(0.1f, state.zoom / 1.12f);
                changed = true;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_0) && !ioT.KeyShift) {
                state.zoom = 1.0f;
                changed = true;
            }
        }
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 innerAvail = ImGui::GetContentRegionAvail();
    const float w = std::max(1.0f, innerAvail.x);
    const float h = std::max(1.0f, innerAvail.y);
    const float labelW = std::min(state.labelWidth, std::max(96.0f, w * 0.42f));
    const float rulerH = 28.0f;
    const float scrollX = ImGui::GetScrollX();
    const ImVec2 windowPos = ImGui::GetWindowPos();
    const ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
    const ImVec2 visibleMin(windowPos.x + contentMin.x, windowPos.y + contentMin.y);
    const float visibleX = visibleMin.x;
    const float visibleTrackX = visibleX + labelW;
    const float visibleTrackW = std::max(1.0f, w - labelW);
    const float durationDrivenWidth = std::clamp(static_cast<float>(state.durationTicks) * 0.12f, visibleTrackW, 60000.0f);
    const float trackW = std::max(visibleTrackW, durationDrivenWidth * state.zoom);
    const float trackX = origin.x + labelW;
    const float trackY = origin.y + rulerH;
    const float contentH =
        std::max(h, rulerH + std::max<size_t>(state.tracks.size(), 1u) * state.rowHeight + ImGui::GetFrameHeightWithSpacing());
    const float trackH = std::max(1.0f, contentH - rulerH - 4.0f);
    const float visibleTop = visibleMin.y;
    const float visibleBottom = visibleMin.y + h;
    const float visibleRight = visibleX + w;
    const ImU32 bg = IM_COL32(22, 22, 27, 255);
    const ImU32 panel = IM_COL32(30, 31, 38, 255);
    const ImU32 grid = IM_COL32(70, 72, 84, 125);
    const ImU32 minorGrid = IM_COL32(54, 56, 66, 90);
    const ImU32 text = IM_COL32(218, 222, 232, 255);
    const ImU32 muted = IM_COL32(135, 140, 154, 255);
    const ImU32 accent = IM_COL32(82, 148, 255, 255);
    const ImU32 keyCol = IM_COL32(245, 184, 72, 255);

    dl->AddRectFilled(ImVec2(visibleX, origin.y), ImVec2(visibleRight, origin.y + h), bg, 4.0f);
    dl->AddRectFilled(ImVec2(visibleX, origin.y), ImVec2(visibleTrackX, origin.y + h), panel, 4.0f, ImDrawFlags_RoundCornersLeft);
    dl->AddLine(ImVec2(visibleTrackX, origin.y), ImVec2(visibleTrackX, origin.y + h), IM_COL32(92, 96, 112, 180), 1.0f);
    dl->AddText(ImVec2(visibleX + 8.0f, origin.y + 7.0f), muted, "Tracks");

    const uint64_t viewSpan = NestedSpanTicks(state);
    uint64_t tickStep;
    uint64_t tickStart;
    uint64_t tickEndIncl;
    if (viewSpan > 0) {
        tickStep = std::max<uint64_t>(1, viewSpan / 10);
        tickStart = state.nestedRangeStartTick;
        tickEndIncl = state.nestedRangeEndTick;
    } else {
        tickStep = std::max<uint64_t>(1, state.durationTicks / 10);
        tickStart = 0;
        tickEndIncl = state.durationTicks;
    }
    for (uint64_t tick = tickStart; tick <= tickEndIncl; tick += tickStep) {
        const float x = TickToX(tick, state, trackX, trackW);
        if (x > trackX + trackW + 1.0f) {
            break;
        }
        if (x < visibleTrackX - 80.0f) {
            continue;
        }
        if (x > visibleRight + 80.0f) {
            break;
        }
        dl->AddLine(ImVec2(x, origin.y + 2.0f), ImVec2(x, origin.y + h - 2.0f), grid, 1.0f);
        char buf[32]{};
        std::snprintf(buf, sizeof(buf), "%llu", static_cast<unsigned long long>(tick));
        dl->AddText(ImVec2(std::max(x + 4.0f, visibleTrackX + 4.0f), origin.y + 7.0f), muted, buf);
        if (tick + tickStep <= tickEndIncl) {
            for (int m = 1; m < 4; ++m) {
                const uint64_t mt = tick + (tickStep * static_cast<uint64_t>(m)) / 4ull;
                if (mt > tickEndIncl) {
                    break;
                }
                const float mx = TickToX(mt, state, trackX, trackW);
                if (mx > visibleTrackX && mx < visibleRight) {
                    dl->AddLine(ImVec2(mx, trackY), ImVec2(mx, origin.y + h - 2.0f), minorGrid, 1.0f);
                }
            }
        }
        if (tickStep == 0 || tickEndIncl - tick < tickStep) {
            break;
        }
    }

    const int trackCount = static_cast<int>(state.tracks.size());
    for (int i = 0; i < trackCount; ++i) {
        const Track& tr = state.tracks[static_cast<size_t>(i)];
        const float y0 = trackY + static_cast<float>(i) * state.rowHeight;
        const float y1 = y0 + state.rowHeight;
        if (y1 < visibleTop || y0 > visibleBottom) {
            continue;
        }
        const bool selected = state.selectedTrack == i;
        const ImU32 rowCol = selected ? IM_COL32(44, 62, 92, 210)
                                      : ((i & 1) ? IM_COL32(27, 28, 34, 255) : IM_COL32(24, 25, 31, 255));
        dl->AddRectFilled(ImVec2(visibleX, y0), ImVec2(visibleRight, y1), rowCol);
        dl->AddText(ImVec2(visibleX + 8.0f, y0 + 4.0f), selected ? text : muted, tr.name.c_str());
        dl->AddLine(ImVec2(visibleX, y1), ImVec2(visibleRight, y1), IM_COL32(60, 62, 72, 120), 1.0f);
        for (size_t k = 0; k < tr.keyTicks.size(); ++k) {
            const float x = TickToX(tr.keyTicks[k], state, trackX, trackW);
            if (x < visibleTrackX - 8.0f || x > visibleRight + 8.0f) {
                continue;
            }
            const bool keySelected = selected && state.selectedKey == static_cast<int>(k);
            DrawDiamond(dl, ImVec2(x, (y0 + y1) * 0.5f), keySelected ? 6.0f : 5.0f, keyCol, IM_COL32(30, 22, 10, 255));
        }
    }

    if (state.tracks.empty()) {
        dl->AddText(ImVec2(visibleTrackX + 10.0f, trackY + 10.0f), muted, "No animation tracks yet. Add keyframes to create tracks.");
    }

    const float playheadX = TickToX(state.playheadTick, state, trackX, trackW);
    dl->AddLine(ImVec2(playheadX, origin.y + 1.0f), ImVec2(playheadX, origin.y + h - 2.0f), accent, 2.0f);
    dl->AddTriangleFilled(ImVec2(playheadX - 6.0f, origin.y + 1.0f), ImVec2(playheadX + 6.0f, origin.y + 1.0f),
        ImVec2(playheadX, origin.y + 12.0f), accent);

    ImGui::SetCursorScreenPos(ImVec2(visibleX, origin.y));
    ImGui::InvisibleButton("timeline_hit", ImVec2(w, std::max(1.0f, h - ImGui::GetFrameHeightWithSpacing())));
    if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        const ImVec2 mp = ImGui::GetIO().MousePos;
        if (mp.x < visibleTrackX) {
            const float relY = mp.y - trackY;
            if (relY >= 0.f && state.rowHeight > 0.f) {
                const int ri = static_cast<int>(relY / state.rowHeight);
                if (ri >= 0 && ri < trackCount) {
                    state.selectedTrack = ri;
                }
            }
        } else {
            const float relY = mp.y - trackY;
            if (relY >= 0.f && state.rowHeight > 0.f) {
                const int ri = static_cast<int>(relY / state.rowHeight);
                if (ri >= 0 && ri < trackCount) {
                    state.selectedTrack = ri;
                    const Track& tr = state.tracks[static_cast<size_t>(ri)];
                    const float mx = mp.x;
                    const float cy = trackY + static_cast<float>(ri) * state.rowHeight + state.rowHeight * 0.5f;
                    int bestK = -1;
                    float bestDist = 1e9f;
                    for (size_t k = 0; k < tr.keyTicks.size(); ++k) {
                        const float kx = TickToX(tr.keyTicks[k], state, trackX, trackW);
                        const float dx = std::fabs(mx - kx);
                        const float dy = std::fabs(mp.y - cy);
                        const float dist = dx + dy * 0.25f;
                        if (dx < 9.f && dy < state.rowHeight * 0.48f && dist < bestDist) {
                            bestDist = dist;
                            bestK = static_cast<int>(k);
                        }
                    }
                    state.selectedKey = bestK;
                }
            }
        }
        if (ImGui::GetIO().MousePos.x >= visibleTrackX) {
            state.playheadTick = XToTick(ImGui::GetIO().MousePos.x, state, trackX, trackW);
        }
        changed = true;
    } else if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        const ImVec2 mp = ImGui::GetIO().MousePos;
        if (mp.x < visibleTrackX) {
            const float relY = mp.y - trackY;
            if (relY >= 0.f && state.rowHeight > 0.f) {
                const int ri = static_cast<int>(relY / state.rowHeight);
                if (ri >= 0 && ri < trackCount) {
                    state.selectedTrack = ri;
                }
            }
        } else {
            const float relY = mp.y - trackY;
            if (relY >= 0.f && state.rowHeight > 0.f) {
                const int ri = static_cast<int>(relY / state.rowHeight);
                if (ri >= 0 && ri < trackCount) {
                    state.selectedTrack = ri;
                    const Track& tr = state.tracks[static_cast<size_t>(ri)];
                    const float mx = mp.x;
                    const float cy = trackY + static_cast<float>(ri) * state.rowHeight + state.rowHeight * 0.5f;
                    int bestK = -1;
                    float bestDist = 1e9f;
                    for (size_t k = 0; k < tr.keyTicks.size(); ++k) {
                        const float kx = TickToX(tr.keyTicks[k], state, trackX, trackW);
                        const float dx = std::fabs(mx - kx);
                        const float dy = std::fabs(mp.y - cy);
                        const float dist = dx + dy * 0.25f;
                        if (dx < 9.f && dy < state.rowHeight * 0.48f && dist < bestDist) {
                            bestDist = dist;
                            bestK = static_cast<int>(k);
                        }
                    }
                    state.selectedKey = bestK;
                }
            }
        }
        if (ImGui::GetIO().MousePos.x >= visibleTrackX) {
            state.playheadTick = XToTick(ImGui::GetIO().MousePos.x, state, trackX, trackW);
        }
        changed = true;
    }

    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(ImVec2(labelW + trackW, contentH));
    ImGui::EndChild();
    ImGui::SetNextItemWidth(110.0f);
    if (ImGui::SliderFloat("Zoom", &state.zoom, 0.1f, 32.0f, "%.2fx")) {
        changed = true;
    }
    ImGui::SameLine();
    if (viewSpan > 0) {
        ImGui::TextDisabled("Ctrl+\xC2\xB1 / wheel / 0  |  tick %llu / %llu  (nested %llu–%llu)", static_cast<unsigned long long>(state.playheadTick),
            static_cast<unsigned long long>(state.durationTicks), static_cast<unsigned long long>(state.nestedRangeStartTick),
            static_cast<unsigned long long>(state.nestedRangeEndTick));
    } else {
        ImGui::TextDisabled("Ctrl+\xC2\xB1 / wheel / 0  |  tick %llu / %llu", static_cast<unsigned long long>(state.playheadTick),
            static_cast<unsigned long long>(state.durationTicks));
    }
    ImGui::PopID();
    (void)style;
    return changed;
}

void DrawTimelineWidget(const char* title, TimelineState& state, bool* visible) {
    if (visible && !(*visible)) {
        return;
    }
    const char* windowTitle = (title && title[0]) ? title : "Timeline";
    if (!ImGui::Begin(windowTitle, visible)) {
        ImGui::End();
        return;
    }

    state.durationTicks = std::max<uint64_t>(state.durationTicks, 1);
    state.playheadTick = std::min(state.playheadTick, state.durationTicks);
    state.zoom = std::clamp(state.zoom, 0.1f, 32.0f);

    DrawAnimationTimeline("timeline_window_canvas", state, ImVec2(0.0f, 220.0f));
    ImGui::End();
}

} // namespace LibUI::Timeline

