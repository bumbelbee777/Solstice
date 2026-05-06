#include "LibUI/Tools/PropertyGrid.hxx"
#include "LibUI/Widgets/Widgets.hxx"

namespace LibUI::Tools {

bool BeginPropertyGrid(const char* id, float labelColumnWidth) {
    if (!id || !Core::IsInitialized()) {
        return false;
    }
    if (!Widgets::BeginTable(id, 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV)) {
        return false;
    }
    Widgets::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, labelColumnWidth);
    Widgets::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    return true;
}

void EndPropertyGrid() {
    Widgets::EndTable();
}

void PropertyLabel(const char* label, const char* help) {
    Widgets::TableNextRow();
    Widgets::TableSetColumnIndex(0);
    Widgets::Text(label ? label : "");
    if (help && help[0] != '\0' && Widgets::IsItemHovered()) {
        Widgets::SetTooltip("%s", help);
    }
    Widgets::TableSetColumnIndex(1);
}

bool PropertyBool(const char* label, bool* value, const char* help) {
    if (!value) {
        return false;
    }
    PropertyLabel(label, help);
    return Widgets::Checkbox("##value", value);
}

bool PropertyInt(const char* label, int* value, int min, int max, const char* help) {
    if (!value) {
        return false;
    }
    PropertyLabel(label, help);
    return Widgets::DragInt("##value", value, 1.0f, min, max);
}

bool PropertyFloat(const char* label, float* value, float speed, float min, float max, const char* format, const char* help) {
    if (!value) {
        return false;
    }
    PropertyLabel(label, help);
    return Widgets::DragFloat("##value", value, speed, min, max, format ? format : "%.3f");
}

bool PropertyFloat3(const char* label, float value[3], float speed, const char* help) {
    if (!value) {
        return false;
    }
    PropertyLabel(label, help);
    return Widgets::DragFloat3("##value", value, speed);
}

} // namespace LibUI::Tools
