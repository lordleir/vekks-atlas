#include "AtlasLocalization.h"
#pragma once
#include <imgui.h>
#include <cmath>

namespace cmm
{
    struct AtlasTheme
    {
        explicit AtlasTheme(float scale)
        {
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(.045f,.061f,.071f,.98f));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(.065f,.085f,.095f,.85f));
            ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(.065f,.085f,.095f,1));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(.40f,.32f,.18f,.65f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.94f,.92f,.85f,1));
            ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(.54f,.61f,.63f,1));
            ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(.06f,.08f,.09f,1));
            ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(.10f,.13f,.14f,1));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(.10f,.13f,.14f,1));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(.16f,.19f,.19f,1));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(.20f,.23f,.21f,1));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(.14f,.18f,.19f,1));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(.28f,.27f,.19f,1));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(.37f,.32f,.19f,1));
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(.30f,.26f,.14f,1));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(.35f,.30f,.17f,1));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(.43f,.35f,.18f,1));
            ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(.89f,.72f,.37f,1));
            ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(.45f,.36f,.20f,.55f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18*scale,16*scale));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(9*scale,6*scale));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8*scale,9*scale));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8*scale);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6*scale);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4*scale);
            ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 6*scale);
        }
        ~AtlasTheme() { ImGui::PopStyleVar(7); ImGui::PopStyleColor(19); }
        AtlasTheme(const AtlasTheme&) = delete;
        AtlasTheme& operator=(const AtlasTheme&) = delete;
    };
    struct EndWindow { ~EndWindow() { ImGui::End(); } };
    struct EndChild { ~EndChild() { ImGui::EndChild(); } };
    struct EndCombo { ~EndCombo() { ImGui::EndCombo(); } };
    struct ScopedID
    {
        explicit ScopedID(const char* id) { ImGui::PushID(id); }
        ~ScopedID() { ImGui::PopID(); }
    };

    inline bool GoldButton(const char* label, ImVec2 size = {})
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(.68f,.50f,.22f,1));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(.81f,.63f,.30f,1));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(.59f,.43f,.20f,1));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.06f,.07f,.07f,1));
        const bool clicked = ImGui::Button(label, size);
        ImGui::PopStyleColor(4);
        return clicked;
    }

    inline bool FolderButton(float scale)
    {
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const bool pressed = ImGui::InvisibleButton("##Folders", {38*scale,30*scale});
        auto* draw = ImGui::GetWindowDrawList();
        const ImU32 gold = IM_COL32(219,180,99,255);
        draw->AddRectFilled(pos, {pos.x+38*scale,pos.y+30*scale},
            ImGui::IsItemHovered() ? IM_COL32(79,70,42,255) : IM_COL32(34,43,45,255), 4*scale);
        draw->AddRectFilled({pos.x+9*scale,pos.y+8*scale}, {pos.x+20*scale,pos.y+13*scale}, gold,2*scale);
        draw->AddRectFilled({pos.x+7*scale,pos.y+12*scale}, {pos.x+31*scale,pos.y+23*scale}, gold,2*scale);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(Tr("Manage folders and markers"));
        return pressed;
    }

    inline bool SettingsButton(float scale)
    {
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const bool pressed = ImGui::InvisibleButton("##Settings", {38*scale,30*scale});
        auto* draw = ImGui::GetWindowDrawList();
        const ImU32 gold = IM_COL32(219,180,99,255);
        draw->AddRectFilled(pos, {pos.x+38*scale,pos.y+30*scale},
            ImGui::IsItemHovered() ? IM_COL32(79,70,42,255) : IM_COL32(34,43,45,255), 4*scale);
        const ImVec2 center{pos.x+19*scale,pos.y+15*scale};
        draw->AddCircle(center, 7*scale, gold, 16, 2.5f*scale);
        draw->AddCircle(center, 2.5f*scale, gold, 12, 2.0f*scale);
        for (int i=0;i<8;++i)
        {
            const float angle = static_cast<float>(i) * 0.785398163f;
            const ImVec2 a{center.x + std::cos(angle)*8*scale, center.y + std::sin(angle)*8*scale};
            const ImVec2 b{center.x + std::cos(angle)*11*scale, center.y + std::sin(angle)*11*scale};
            draw->AddLine(a,b,gold,2.5f*scale);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(Tr("Map and minimap settings"));
        return pressed;
    }

    inline bool EyeButton(const char* id, bool visible, float scale)
    {
        ImGui::PushID(id);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const bool pressed = ImGui::InvisibleButton("##Eye", {24*scale, 20*scale});
        auto* draw = ImGui::GetWindowDrawList();
        const ImU32 color = visible ? IM_COL32(219,180,99,255) : IM_COL32(104,113,114,210);
        const ImVec2 center{pos.x+12*scale,pos.y+10*scale};
        draw->AddBezierCurve({pos.x+3*scale,center.y}, {pos.x+7*scale,pos.y+3*scale},
            {pos.x+17*scale,pos.y+3*scale}, {pos.x+21*scale,center.y}, color, 1.6f*scale);
        draw->AddBezierCurve({pos.x+3*scale,center.y}, {pos.x+7*scale,pos.y+17*scale},
            {pos.x+17*scale,pos.y+17*scale}, {pos.x+21*scale,center.y}, color, 1.6f*scale);
        if (visible) draw->AddCircleFilled(center, 3*scale, color, 12);
        else draw->AddLine({pos.x+4*scale,pos.y+17*scale},{pos.x+20*scale,pos.y+3*scale},color,1.8f*scale);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", Tr("Show or hide this folder's markers"));
        ImGui::PopID();
        return pressed;
    }
}
