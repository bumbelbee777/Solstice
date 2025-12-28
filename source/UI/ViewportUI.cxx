#include <UI/ViewportUI.hxx>
#include <UI/UISystem.hxx>
#include <imgui.h>
#include <imgui_internal.h>
#include <cmath>

namespace Solstice::UI::ViewportUI {

    // Utility function to project 3D point to screen space
    Math::Vec2 ProjectToScreen(const Math::Vec3& WorldPos, const Math::Matrix4& ViewMatrix, const Math::Matrix4& ProjectionMatrix, int ScreenWidth, int ScreenHeight) {
        // Transform to clip space
        Math::Vec4 clipPos = ProjectionMatrix * (ViewMatrix * Math::Vec4(WorldPos.x, WorldPos.y, WorldPos.z, 1.0f));

        // specific check for points behind camera
        if (clipPos.w <= 0.0f) {
           return Math::Vec2(-10000.0f, -10000.0f);
        }

        // Perspective divide
        if (std::abs(clipPos.w) > 0.0001f) {
            clipPos.x /= clipPos.w;
            clipPos.y /= clipPos.w;
            clipPos.z /= clipPos.w;
        }

        // NDC to screen space
        float screenX = (clipPos.x + 1.0f) * 0.5f * (float)ScreenWidth;
        float screenY = (1.0f - clipPos.y) * 0.5f * (float)ScreenHeight;

        return Math::Vec2(screenX, screenY);
    }

    // Utility function to calculate anchor position
    ::ImVec2 CalculateAnchorPosition(Anchor AnchorPos, float Width, float Height, int ScreenWidth, int ScreenHeight, float OffsetX, float OffsetY) {
        float x = 0.0f;
        float y = 0.0f;

        switch (AnchorPos) {
            case Anchor::TopLeft:
                x = OffsetX;
                y = OffsetY;
                break;
            case Anchor::TopCenter:
                x = ((float)ScreenWidth - Width) * 0.5f + OffsetX;
                y = OffsetY;
                break;
            case Anchor::TopRight:
                x = (float)ScreenWidth - Width - OffsetX;
                y = OffsetY;
                break;
            case Anchor::CenterLeft:
                x = OffsetX;
                y = ((float)ScreenHeight - Height) * 0.5f + OffsetY;
                break;
            case Anchor::Center:
                x = ((float)ScreenWidth - Width) * 0.5f + OffsetX;
                y = ((float)ScreenHeight - Height) * 0.5f + OffsetY;
                break;
            case Anchor::CenterRight:
                x = (float)ScreenWidth - Width - OffsetX;
                y = ((float)ScreenHeight - Height) * 0.5f + OffsetY;
                break;
            case Anchor::BottomLeft:
                x = OffsetX;
                y = (float)ScreenHeight - Height - OffsetY;
                break;
            case Anchor::BottomCenter:
                x = ((float)ScreenWidth - Width) * 0.5f + OffsetX;
                y = (float)ScreenHeight - Height - OffsetY;
                break;
            case Anchor::BottomRight:
                x = (float)ScreenWidth - Width - OffsetX;
                y = (float)ScreenHeight - Height - OffsetY;
                break;
        }

        return ::ImVec2(x, y);
    }

    // WorldSpaceDialog implementation
    WorldSpaceDialog::WorldSpaceDialog(const Math::Vec3& Position, float Width, float Height)
        : m_Position(Position), m_Width(Width), m_Height(Height) {
    }

    WorldSpaceDialog::~WorldSpaceDialog() {
    }

    void WorldSpaceDialog::Render(const Math::Matrix4& ViewMatrix, const Math::Matrix4& ProjectionMatrix, int ScreenWidth, int ScreenHeight) {
        if (!m_Visible) return;

        // Project 3D position to screen space
        Math::Vec2 screenPos = ProjectToScreen(m_Position, ViewMatrix, ProjectionMatrix, ScreenWidth, ScreenHeight);

        // Check if position is on screen
        if (screenPos.x < -m_Width || screenPos.x > ScreenWidth + m_Width ||
            screenPos.y < -m_Height || screenPos.y > ScreenHeight + m_Height) {
            return; // Off screen
        }

        // Set next window position
        ImGui::SetNextWindowPos(ImVec2(screenPos.x, screenPos.y), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(m_Width, m_Height), ImGuiCond_Always);

        // Create window flags for billboard-like behavior
        // Removed ImGuiWindowFlags_NoBackground to make the dialog visible
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar;

        if (ImGui::Begin("##WorldSpaceDialog", nullptr, flags)) {
            if (m_ContentCallback) {
                m_ContentCallback();
            }
        }
        ImGui::End();
    }

    // WorldSpaceLabel implementation
    WorldSpaceLabel::WorldSpaceLabel(const Math::Vec3& Position, const std::string& Text)
        : m_Position(Position), m_Text(Text) {
    }

    WorldSpaceLabel::~WorldSpaceLabel() {
    }

    void WorldSpaceLabel::Render(const Math::Matrix4& ViewMatrix, const Math::Matrix4& ProjectionMatrix, int ScreenWidth, int ScreenHeight) {
        if (!m_Visible || m_Text.empty()) return;

        // Project 3D position to screen space
        Math::Vec2 screenPos = ProjectToScreen(m_Position, ViewMatrix, ProjectionMatrix, ScreenWidth, ScreenHeight);

        // Check if position is on screen
        ImVec2 textSize = ImGui::CalcTextSize(m_Text.c_str());
        if (screenPos.x < -textSize.x || screenPos.x > ScreenWidth + textSize.x ||
            screenPos.y < -textSize.y || screenPos.y > ScreenHeight + textSize.y) {
            return; // Off screen
        }

        // Draw text at screen position using foreground draw list
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        if (drawList) {
            drawList->AddText(ImVec2(screenPos.x, screenPos.y), ImGui::GetColorU32(ImGuiCol_Text), m_Text.c_str());
        }
    }

    // WorldSpaceButton implementation
    WorldSpaceButton::WorldSpaceButton(const Math::Vec3& Position, float Width, float Height, const std::string& Label)
        : m_Position(Position), m_Width(Width), m_Height(Height), m_Label(Label) {
    }

    WorldSpaceButton::~WorldSpaceButton() {
    }

    bool WorldSpaceButton::Render(const Math::Matrix4& ViewMatrix, const Math::Matrix4& ProjectionMatrix, int ScreenWidth, int ScreenHeight) {
        if (!m_Visible) return false;

        // Project 3D position to screen space
        Math::Vec2 screenPos = ProjectToScreen(m_Position, ViewMatrix, ProjectionMatrix, ScreenWidth, ScreenHeight);

        // Check if position is on screen
        if (screenPos.x < -m_Width || screenPos.x > ScreenWidth + m_Width ||
            screenPos.y < -m_Height || screenPos.y > ScreenHeight + m_Height) {
            return false; // Off screen
        }

        // Set next window position
        ImGui::SetNextWindowPos(ImVec2(screenPos.x, screenPos.y), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(m_Width, m_Height), ImGuiCond_Always);

        // Create window flags
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                                 ImGuiWindowFlags_NoBackground;

        bool clicked = false;
        if (ImGui::Begin("##WorldSpaceButton", nullptr, flags)) {
            if (ImGui::Button(m_Label.c_str(), ImVec2(m_Width - 20.0f, m_Height - 20.0f))) {
                clicked = true;
                if (m_OnClick) {
                    m_OnClick();
                }
            }
        }
        ImGui::End();

        return clicked;
    }

    // OverlayDialog implementation
    OverlayDialog::OverlayDialog(const std::string& Title, Anchor AnchorPos, float Width, float Height)
        : m_Title(Title), m_Anchor(AnchorPos), m_Width(Width), m_Height(Height) {
    }

    OverlayDialog::~OverlayDialog() {
    }

    void OverlayDialog::SetPosition(Anchor AnchorPos, float OffsetX, float OffsetY) {
        m_Anchor = AnchorPos;
        m_OffsetX = OffsetX;
        m_OffsetY = OffsetY;
    }

    bool OverlayDialog::Begin() {
        if (!m_Visible) return false;

        ImGuiIO& io = ImGui::GetIO();
        ImVec2 pos = CalculateAnchorPosition(m_Anchor, m_Width, m_Height, (int)io.DisplaySize.x, (int)io.DisplaySize.y, m_OffsetX, m_OffsetY);

        ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(m_Width, m_Height), ImGuiCond_Always);

        bool opened = ImGui::Begin(m_Title.c_str(), nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        if (opened && m_ContentCallback) {
            m_ContentCallback();
        }

        return opened;
    }

    void OverlayDialog::End() {
        ImGui::End();
    }

    // OverlayList implementation
    OverlayList::OverlayList(const std::string& Id, Anchor AnchorPos, float Width, float Height)
        : m_Id(Id), m_Anchor(AnchorPos), m_Width(Width), m_Height(Height) {
    }

    OverlayList::~OverlayList() {
    }

    void OverlayList::SetItems(const std::vector<std::string>& Items) {
        m_Items = Items;
        if (m_SelectedIndex >= (int)m_Items.size()) {
            m_SelectedIndex = -1;
        }
    }

    void OverlayList::AddItem(const std::string& Item) {
        m_Items.push_back(Item);
    }

    void OverlayList::ClearItems() {
        m_Items.clear();
        m_SelectedIndex = -1;
    }

    std::string OverlayList::GetSelectedItem() const {
        if (m_SelectedIndex >= 0 && m_SelectedIndex < (int)m_Items.size()) {
            return m_Items[m_SelectedIndex];
        }
        return "";
    }

    void OverlayList::SetPosition(Anchor AnchorPos, float OffsetX, float OffsetY) {
        m_Anchor = AnchorPos;
        m_OffsetX = OffsetX;
        m_OffsetY = OffsetY;
    }

    void OverlayList::Render() {
        if (!m_Visible) return;

        ImGuiIO& io = ImGui::GetIO();
        ImVec2 pos = CalculateAnchorPosition(m_Anchor, m_Width, m_Height, (int)io.DisplaySize.x, (int)io.DisplaySize.y, m_OffsetX, m_OffsetY);

        ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(m_Width, m_Height), ImGuiCond_Always);

        if (ImGui::Begin(m_Id.c_str(), nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar)) {
            for (size_t i = 0; i < m_Items.size(); ++i) {
                bool selected = (m_SelectedIndex == (int)i);
                if (ImGui::Selectable(m_Items[i].c_str(), selected)) {
                    int oldIndex = m_SelectedIndex;
                    m_SelectedIndex = (int)i;
                    if (m_OnSelectionChanged && oldIndex != m_SelectedIndex) {
                        m_OnSelectionChanged(m_SelectedIndex, m_Items[i]);
                    }
                }
            }
        }
        ImGui::End();
    }

    // OverlaySlider implementation
    OverlaySlider::OverlaySlider(const std::string& Label, float& Value, float Min, float Max, Anchor AnchorPos)
        : m_Label(Label), m_Value(&Value), m_Min(Min), m_Max(Max), m_Anchor(AnchorPos) {
    }

    OverlaySlider::~OverlaySlider() {
    }

    void OverlaySlider::SetPosition(Anchor AnchorPos, float OffsetX, float OffsetY) {
        m_Anchor = AnchorPos;
        m_OffsetX = OffsetX;
        m_OffsetY = OffsetY;
    }

    bool OverlaySlider::Render() {
        if (!m_Visible || !m_Value) return false;

        ImGuiIO& io = ImGui::GetIO();
        float width = 200.0f; // Default slider width
        float height = 50.0f; // Default height for label + slider

        ImVec2 textSize = ImGui::CalcTextSize(m_Label.c_str());
        width = (std::max)(width, textSize.x + 20.0f);

        ImVec2 pos = CalculateAnchorPosition(m_Anchor, width, height, (int)io.DisplaySize.x, (int)io.DisplaySize.y, m_OffsetX, m_OffsetY);

        ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);

        bool changed = false;
        if (ImGui::Begin(("##OverlaySlider_" + m_Label).c_str(), nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground)) {
            ImGui::Text("%s", m_Label.c_str());

            ImGuiSliderFlags flags = 0;
            if (m_Step > 0.0f) {
                flags |= ImGuiSliderFlags_AlwaysClamp;
            }

            if (m_Step > 0.0f) {
                changed = ImGui::SliderFloat("##slider", m_Value, m_Min, m_Max, "%.2f", flags);
                // Apply step
                *m_Value = std::round(*m_Value / m_Step) * m_Step;
                *m_Value = std::max(m_Min, std::min(m_Max, *m_Value));
            } else {
                changed = ImGui::SliderFloat("##slider", m_Value, m_Min, m_Max, "%.2f", flags);
            }

            if (m_ShowValue) {
                ImGui::SameLine();
                ImGui::Text("%.2f", *m_Value);
            }
        }
        ImGui::End();

        return changed;
    }

    // OverlayPanel implementation
    OverlayPanel::OverlayPanel(const std::string& Id, Anchor AnchorPos, float Width, float Height)
        : m_Id(Id), m_Anchor(AnchorPos), m_Width(Width), m_Height(Height) {
    }

    OverlayPanel::~OverlayPanel() {
    }

    void OverlayPanel::SetPosition(Anchor AnchorPos, float OffsetX, float OffsetY) {
        m_Anchor = AnchorPos;
        m_OffsetX = OffsetX;
        m_OffsetY = OffsetY;
    }

    void OverlayPanel::Begin() {
        if (!m_Visible) return;

        ImGuiIO& io = ImGui::GetIO();
        ImVec2 pos = CalculateAnchorPosition(m_Anchor, m_Width, m_Height, (int)io.DisplaySize.x, (int)io.DisplaySize.y, m_OffsetX, m_OffsetY);

        ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(m_Width, m_Height), ImGuiCond_Always);

        ImGui::Begin(m_Id.c_str(), nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        if (m_ContentCallback) {
            m_ContentCallback();
        }
    }

    void OverlayPanel::End() {
        ImGui::End();
    }

} // namespace Solstice::UI::ViewportUI
