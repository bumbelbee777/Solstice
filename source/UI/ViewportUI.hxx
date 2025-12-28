#pragma once

#include <imgui.h>
#include "../Solstice.hxx"
#include <Math/Vector.hxx>
#include <Math/Matrix.hxx>
#include <string>
#include <functional>
#include <memory>

// Forward declarations
struct ImDrawList;

namespace Solstice::UI::ViewportUI {

    // Anchor position for screen-space overlays
    enum class Anchor {
        TopLeft,
        TopCenter,
        TopRight,
        CenterLeft,
        Center,
        CenterRight,
        BottomLeft,
        BottomCenter,
        BottomRight
    };

    // 3D World-Space UI Elements

    // World-space dialog box (billboard in 3D space)
    class SOLSTICE_API WorldSpaceDialog {
    public:
        WorldSpaceDialog(const Math::Vec3& Position, float Width, float Height);
        ~WorldSpaceDialog();

        void SetPosition(const Math::Vec3& Position) { m_Position = Position; }
        Math::Vec3 GetPosition() const { return m_Position; }

        void SetSize(float Width, float Height) { m_Width = Width; m_Height = Height; }
        void GetSize(float& Width, float& Height) const { Width = m_Width; Height = m_Height; }

        void SetVisible(bool Visible) { m_Visible = Visible; }
        bool IsVisible() const { return m_Visible; }

        void SetDepthTest(bool Enable) { m_DepthTest = Enable; }
        bool GetDepthTest() const { return m_DepthTest; }

        // Render the dialog (call from render loop)
        void Render(const Math::Matrix4& ViewMatrix, const Math::Matrix4& ProjectionMatrix, int ScreenWidth, int ScreenHeight);

        // Content callback - called during render to draw ImGui content
        void SetContentCallback(std::function<void()> Callback) { m_ContentCallback = Callback; }

    private:
        Math::Vec3 m_Position;
        float m_Width;
        float m_Height;
        bool m_Visible{true};
        bool m_DepthTest{true};
        std::function<void()> m_ContentCallback;
    };

    // World-space text label
    class WorldSpaceLabel {
    public:
        WorldSpaceLabel(const Math::Vec3& Position, const std::string& Text);
        ~WorldSpaceLabel();

        void SetPosition(const Math::Vec3& Position) { m_Position = Position; }
        Math::Vec3 GetPosition() const { return m_Position; }

        void SetText(const std::string& Text) { m_Text = Text; }
        std::string GetText() const { return m_Text; }

        void SetVisible(bool Visible) { m_Visible = Visible; }
        bool IsVisible() const { return m_Visible; }

        void Render(const Math::Matrix4& ViewMatrix, const Math::Matrix4& ProjectionMatrix, int ScreenWidth, int ScreenHeight);

    private:
        Math::Vec3 m_Position;
        std::string m_Text;
        bool m_Visible{true};
    };

    // World-space button (interactive button in 3D space)
    class WorldSpaceButton {
    public:
        WorldSpaceButton(const Math::Vec3& Position, float Width, float Height, const std::string& Label);
        ~WorldSpaceButton();

        void SetPosition(const Math::Vec3& Position) { m_Position = Position; }
        Math::Vec3 GetPosition() const { return m_Position; }

        void SetSize(float Width, float Height) { m_Width = Width; m_Height = Height; }
        void GetSize(float& Width, float& Height) const { Width = m_Width; Height = m_Height; }

        void SetLabel(const std::string& Label) { m_Label = Label; }
        std::string GetLabel() const { return m_Label; }

        void SetVisible(bool Visible) { m_Visible = Visible; }
        bool IsVisible() const { return m_Visible; }

        void SetOnClick(std::function<void()> Callback) { m_OnClick = Callback; }

        // Returns true if button was clicked this frame
        bool Render(const Math::Matrix4& ViewMatrix, const Math::Matrix4& ProjectionMatrix, int ScreenWidth, int ScreenHeight);

    private:
        Math::Vec3 m_Position;
        float m_Width;
        float m_Height;
        std::string m_Label;
        bool m_Visible{true};
        std::function<void()> m_OnClick;
    };

    // Screen-Space Overlay Elements

    // Overlay dialog (screen-space dialog with anchoring)
    class OverlayDialog {
    public:
        OverlayDialog(const std::string& Title, Anchor AnchorPos = Anchor::Center, float Width = 400.0f, float Height = 300.0f);
        ~OverlayDialog();

        void SetTitle(const std::string& Title) { m_Title = Title; }
        void SetPosition(Anchor AnchorPos, float OffsetX = 0.0f, float OffsetY = 0.0f);
        void SetSize(float Width, float Height) { m_Width = Width; m_Height = Height; }

        void SetVisible(bool Visible) { m_Visible = Visible; }
        bool IsVisible() const { return m_Visible; }

        void SetContentCallback(std::function<void()> Callback) { m_ContentCallback = Callback; }

        // Call Begin/End in render loop
        bool Begin();
        void End();

    private:
        std::string m_Title;
        Anchor m_Anchor;
        float m_OffsetX{0.0f};
        float m_OffsetY{0.0f};
        float m_Width;
        float m_Height;
        bool m_Visible{true};
        std::function<void()> m_ContentCallback;
    };

    // Overlay list (scrollable, selectable list)
    class OverlayList {
    public:
        OverlayList(const std::string& Id, Anchor AnchorPos = Anchor::CenterLeft, float Width = 200.0f, float Height = 300.0f);
        ~OverlayList();

        void SetItems(const std::vector<std::string>& Items);
        void AddItem(const std::string& Item);
        void ClearItems();

        int GetSelectedIndex() const { return m_SelectedIndex; }
        std::string GetSelectedItem() const;

        void SetPosition(Anchor AnchorPos, float OffsetX = 0.0f, float OffsetY = 0.0f);
        void SetSize(float Width, float Height) { m_Width = Width; m_Height = Height; }

        void SetVisible(bool Visible) { m_Visible = Visible; }
        bool IsVisible() const { return m_Visible; }

        void SetOnSelectionChanged(std::function<void(int Index, const std::string& Item)> Callback) { m_OnSelectionChanged = Callback; }

        // Call in render loop
        void Render();

    private:
        std::string m_Id;
        Anchor m_Anchor;
        float m_OffsetX{0.0f};
        float m_OffsetY{0.0f};
        float m_Width;
        float m_Height;
        bool m_Visible{true};
        std::vector<std::string> m_Items;
        int m_SelectedIndex{-1};
        std::function<void(int, const std::string&)> m_OnSelectionChanged;
    };

    // Overlay slider (enhanced slider with value display)
    class OverlaySlider {
    public:
        OverlaySlider(const std::string& Label, float& Value, float Min, float Max, Anchor AnchorPos = Anchor::Center);
        ~OverlaySlider();

        void SetPosition(Anchor AnchorPos, float OffsetX = 0.0f, float OffsetY = 0.0f);
        void SetShowValue(bool Show) { m_ShowValue = Show; }
        void SetStep(float Step) { m_Step = Step; }

        void SetVisible(bool Visible) { m_Visible = Visible; }
        bool IsVisible() const { return m_Visible; }

        // Call in render loop
        bool Render();

    private:
        std::string m_Label;
        float* m_Value;
        float m_Min;
        float m_Max;
        float m_Step{0.0f};
        Anchor m_Anchor;
        float m_OffsetX{0.0f};
        float m_OffsetY{0.0f};
        bool m_ShowValue{true};
        bool m_Visible{true};
    };

    // Overlay panel (container for grouping overlay elements)
    class OverlayPanel {
    public:
        OverlayPanel(const std::string& Id, Anchor AnchorPos = Anchor::TopLeft, float Width = 200.0f, float Height = 200.0f);
        ~OverlayPanel();

        void SetPosition(Anchor AnchorPos, float OffsetX = 0.0f, float OffsetY = 0.0f);
        void SetSize(float Width, float Height) { m_Width = Width; m_Height = Height; }

        void SetVisible(bool Visible) { m_Visible = Visible; }
        bool IsVisible() const { return m_Visible; }

        void SetContentCallback(std::function<void()> Callback) { m_ContentCallback = Callback; }

        // Call Begin/End in render loop
        void Begin();
        void End();

    private:
        std::string m_Id;
        Anchor m_Anchor;
        float m_OffsetX{0.0f};
        float m_OffsetY{0.0f};
        float m_Width;
        float m_Height;
        bool m_Visible{true};
        std::function<void()> m_ContentCallback;
    };

    // Utility function to project 3D point to screen space
    SOLSTICE_API Math::Vec2 ProjectToScreen(const Math::Vec3& WorldPos, const Math::Matrix4& ViewMatrix, const Math::Matrix4& ProjectionMatrix, int ScreenWidth, int ScreenHeight);

    // Utility function to calculate anchor position
    SOLSTICE_API ::ImVec2 CalculateAnchorPosition(Anchor AnchorPos, float Width, float Height, int ScreenWidth, int ScreenHeight, float OffsetX = 0.0f, float OffsetY = 0.0f);

} // namespace Solstice::UI::ViewportUI
