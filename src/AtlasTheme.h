#include "AtlasLocalization.h"
#pragma once
#include <imgui.h>
#include <imgui_internal.h>
#include <cmath>
#include <algorithm>

namespace cmm
{
    // Menu scale is independent of world/map markers and capped for compact game overlays.
    inline float AtlasUiScale(float gameScale) { return std::clamp(gameScale, .85f, 1.0f); }

    inline void SetNextAtlasAutoHeightWindowSize(float preferredWidth)
    {
        const auto display=ImGui::GetIO().DisplaySize;
        const float width=std::floor(std::min(preferredWidth,std::max(100.f,display.x-16.f)));
        // Wrapped text and full-width controls depend on the window width.
        // Constrain it on every frame so auto-resize cannot feed back into itself.
        ImGui::SetNextWindowSize({width,0},ImGuiCond_Appearing);
        ImGui::SetNextWindowSizeConstraints({width,0},{width,std::max(100.f,display.y-16.f)});
    }

    struct AtlasTheme
    {
        ImDrawListFlags previousDrawFlags{};
        explicit AtlasTheme(float scale)
        {
            auto* shared=ImGui::GetDrawListSharedData();previousDrawFlags=shared->InitialFlags;
            shared->InitialFlags|=ImDrawListFlags_AntiAliasedLines|ImDrawListFlags_AntiAliasedFill;
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(.043f,.110f,.200f,.98f));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(.039f,.102f,.184f,.98f));
            ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(.035f,.065f,.115f,1));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(.25f,.34f,.49f,.75f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.89f,.92f,1,1));
            ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(.53f,.61f,.75f,1));
            ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(.035f,.060f,.105f,1));
            ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(.055f,.10f,.17f,1));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(.047f,.122f,.224f,1));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(.09f,.16f,.26f,1));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(.12f,.20f,.31f,1));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(.075f,.18f,.315f,1));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(.13f,.22f,.34f,1));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(.22f,.26f,.30f,1));
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(.30f,.26f,.14f,1));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(.35f,.30f,.17f,1));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(.43f,.35f,.18f,1));
            ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(.89f,.72f,.37f,1));
            ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(.45f,.36f,.20f,.55f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10*scale,9*scale));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(7*scale,4*scale));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6*scale,5*scale));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 9*scale);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6*scale);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6*scale);
            ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 6*scale);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        }
        ~AtlasTheme() { ImGui::PopStyleVar(8); ImGui::PopStyleColor(19); ImGui::GetDrawListSharedData()->InitialFlags=previousDrawFlags; }
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

    enum class AtlasGlyph { None, Route, Pencil, Eraser, Pin, Folder, Settings, Copy, Paste, Trash, Stop };

    inline void DrawAtlasGlyph(ImDrawList* d, ImVec2 c, float r, AtlasGlyph glyph, ImU32 color)
    {
        const auto p=[&](float x,float y){return ImVec2{c.x+x*r,c.y+y*r};};
        const float stroke=std::max(1.0f,r*.15f);
        switch (glyph)
        {
        case AtlasGlyph::None: break;
        case AtlasGlyph::Route:
            d->AddBezierCurve(p(-.8f,.6f),p(-.4f,-1.4f),p(.4f,1.4f),p(.8f,-.6f),color,stroke*1.5f);
            d->AddCircleFilled(p(-.8f,.6f),r*.22f,color); d->AddCircleFilled(p(.8f,-.6f),r*.22f,color); break;
        case AtlasGlyph::Pencil:
            d->AddLine(p(-.45f,.45f),p(.65f,-.65f),color,stroke*3);
            d->AddTriangleFilled(p(-.85f,.85f),p(-.7f,.15f),p(-.15f,.7f),color); break;
        case AtlasGlyph::Eraser:
            d->AddQuadFilled(p(-.85f,.25f),p(.2f,-.8f),p(.85f,-.15f),p(-.2f,.9f),color);
            d->AddLine(p(-.5f,-.1f),p(.2f,.6f),IM_COL32(20,32,52,255),stroke); break;
        case AtlasGlyph::Pin:
            d->AddCircleFilled(p(0,-.3f),r*.65f,color); d->AddTriangleFilled(p(-.5f,0),p(.5f,0),p(0,1),color);
            d->AddCircleFilled(p(0,-.3f),r*.23f,IM_COL32(20,32,52,255)); break;
        case AtlasGlyph::Folder:
            d->AddRectFilled(p(-.85f,-.35f),p(.85f,.65f),color,r*.12f);
            d->AddRectFilled(p(-.85f,-.6f),p(-.1f,0),color,r*.1f); break;
        case AtlasGlyph::Settings:
            d->AddCircle(c,r*.55f,color,20,stroke*1.8f);
            for(int i=0;i<8;++i) {float a=i*.785398f; d->AddLine(p(std::cos(a)*.6f,std::sin(a)*.6f),p(std::cos(a)*.9f,std::sin(a)*.9f),color,stroke*1.8f);} break;
        case AtlasGlyph::Trash:
            d->AddRect(p(-.5f,-.35f),p(.5f,.8f),color,r*.1f,ImDrawCornerFlags_All,stroke);
            d->AddLine(p(-.75f,-.5f),p(.75f,-.5f),color,stroke); d->AddLine(p(-.25f,-.8f),p(.25f,-.8f),color,stroke);
            d->AddLine(p(-.18f,-.15f),p(-.18f,.6f),color,stroke); d->AddLine(p(.18f,-.15f),p(.18f,.6f),color,stroke); break;
        case AtlasGlyph::Stop: d->AddRectFilled(p(-.65f,-.65f),p(.65f,.65f),color,r*.1f); break;
        case AtlasGlyph::Copy:
            d->AddRect(p(-.7f,-.35f),p(.3f,.8f),color,r*.1f,ImDrawCornerFlags_All,stroke);
            d->AddRectFilled(p(-.3f,-.8f),p(.7f,.35f),color,r*.1f); break;
        case AtlasGlyph::Paste:
            d->AddRect(p(-.65f,-.6f),p(.65f,.85f),color,r*.1f,ImDrawCornerFlags_All,stroke);
            d->AddRectFilled(p(-.3f,-.85f),p(.3f,-.4f),color,r*.1f); break;
        }
    }

    inline bool AtlasTile(const char* id, const char* label, AtlasGlyph glyph, ImVec2 size, bool selected=false, bool danger=false, bool stacked=false)
    {
        const auto at=ImGui::GetCursorScreenPos();
        const bool clicked=ImGui::InvisibleButton(id,size);
        const bool hovered=ImGui::IsItemHovered();
        auto* d=ImGui::GetWindowDrawList();
        const ImVec2 end{at.x+size.x,at.y+size.y};
        const ImU32 edge=selected?IM_COL32(244,197,84,255):IM_COL32(65,89,124,hovered?255:160);
        d->AddRectFilled(at,end,selected?IM_COL32(56,54,33,240):hovered?IM_COL32(28,49,78,255):IM_COL32(16,29,50,245),5);
        if(selected) d->AddRect({at.x-1,at.y-1},{end.x+1,end.y+1},IM_COL32(244,197,84,45),6,ImDrawCornerFlags_All,5);
        d->AddRect(at,end,edge,5,ImDrawCornerFlags_All,selected?1.8f:1.f);
        const ImU32 ink=danger?IM_COL32(245,104,102,255):selected?IM_COL32(255,218,119,255):IM_COL32(193,209,241,255);
        if(label && *label)
        {
            const auto text=ImGui::CalcTextSize(label);
            const bool tall=stacked;
            const ImVec2 center=tall?ImVec2{at.x+size.x*.5f,at.y+size.y*.30f}:ImVec2{at.x+15,at.y+size.y*.5f};
            DrawAtlasGlyph(d,center,tall?9.f:7.f,glyph,ink);
            const ImVec2 textAt=tall?ImVec2{at.x+(size.x-text.x)*.5f,end.y-text.y-4}:ImVec2{at.x+29,at.y+(size.y-text.y)*.5f};
            d->PushClipRect(at,end,true); d->AddText(textAt,ink,label); d->PopClipRect();
        }
        else DrawAtlasGlyph(d,{at.x+size.x*.5f,at.y+size.y*.5f},size.y*.28f,glyph,ink);
        return clicked;
    }

    inline void AtlasHeading(const char* text)
    {
        const auto at=ImGui::GetCursorScreenPos();
        const float size=ImGui::GetFontSize()*1.05f;
        ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(),size,at,IM_COL32(228,236,250,255),text);
        ImGui::Dummy({0,size+1});
    }

    inline void AtlasWindowAccent()
    {
        const auto pos=ImGui::GetWindowPos(),size=ImGui::GetWindowSize(); auto* d=ImGui::GetWindowDrawList();
        const float y=pos.y+ImGui::GetFontSize()+ImGui::GetStyle().FramePadding.y*2;
        d->AddLine({pos.x+12,y},{pos.x+size.x-12,y},IM_COL32(206,166,88,150),1);
        for(float x:{pos.x+12,pos.x+size.x-12})
        { d->AddLine({x-4,y},{x,y-4},IM_COL32(235,194,110,200)); d->AddLine({x,y-4},{x+4,y},IM_COL32(235,194,110,200)); }
    }

    inline bool AtlasWindowHeader(const char* title, bool& visible, float scale, AtlasGlyph glyph, ImTextureID emblem = nullptr)
    {
        auto* d=ImGui::GetWindowDrawList();
        // Keep ImGui's rounded native background and scrollbar rendering.
        d->Flags |= ImDrawListFlags_AntiAliasedLines | ImDrawListFlags_AntiAliasedFill;
        const auto at=ImGui::GetCursorScreenPos();
        const float h=28*scale,close=22*scale,available=ImGui::GetContentRegionAvail().x;
        ImGui::InvisibleButton("##drag-title",{std::max(1.f,available-close-8*scale),h});
        if(ImGui::IsItemHovered() && ImGui::IsMouseClicked(0))
            ImGui::StartMouseMovingWindow(ImGui::GetCurrentWindow());
        if(emblem) d->AddImage(emblem,{at.x,at.y},{at.x+28*scale,at.y+28*scale});
        else DrawAtlasGlyph(d,{at.x+12*scale,at.y+14*scale},10*scale,glyph,IM_COL32(244,208,125,255));
        const float font=ImGui::GetFontSize()*1.2f;
        d->PushClipRect({at.x+34*scale,at.y},{at.x+available-close-8*scale,at.y+h},true);
        d->AddText(ImGui::GetFont(),font,{at.x+34*scale,at.y+(h-font)*.5f},emblem?IM_COL32(242,214,164,255):IM_COL32(236,240,255,255),title);
        d->PopClipRect(); ImGui::SameLine();
        const auto x=ImGui::GetCursorScreenPos();
        if(ImGui::InvisibleButton("##close-window",{close,h}))visible=false;
        const auto ink=ImGui::IsItemHovered()?IM_COL32(255,210,129,255):IM_COL32(209,218,235,255);
        const ImVec2 c{x.x+close*.5f,x.y+h*.5f};
        d->AddLine({c.x-5*scale,c.y-5*scale},{c.x+5*scale,c.y+5*scale},ink,1.8f*scale);
        d->AddLine({c.x+5*scale,c.y-5*scale},{c.x-5*scale,c.y+5*scale},ink,1.8f*scale);
        return visible;
    }

    inline bool GoldButton(const char* label, ImVec2 size = {})
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(.36f,.28f,.16f,1));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(.46f,.36f,.20f,1));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(.30f,.24f,.14f,1));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(.78f,.63f,.36f,1));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1,.95f,.78f,1));
        const bool clicked=ImGui::Button(label,size);
        ImGui::PopStyleColor(5);
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
