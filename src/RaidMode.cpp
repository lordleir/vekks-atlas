#include "RaidMode.h"
#include "AtlasClipboard.h"
#include "AtlasLocalization.h"
#include "AtlasTheme.h"
#include "MarkerLink.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <unordered_set>

namespace cmm
{
    namespace
    {
        using json=nlohmann::json;
        constexpr size_t MaxPaths=128, MaxPoints=4096, MaxTotalPoints=65536;
        std::string Id()
        {
            static uint64_t serial{};
            return "route-"+std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count())+
                "-"+std::to_string(++serial);
        }
        float Distance(WorldVec3 a,WorldVec3 b)
        { return std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z)); }
        bool Valid(WorldVec3 p)
        { return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::abs(p.x)<100000&&std::abs(p.y)<100000&&std::abs(p.z)<100000; }
        size_t PointCount(const std::vector<RaidPath>& paths)
        { size_t n=0; for (const auto& p:paths) n+=p.points.size(); return n; }
        json Encode(const RaidPath& p,bool share)
        {
            json item{{"map",p.mapId},{"name",p.name},{"color",p.color},{"visible",p.visible},{"points",json::array()}};
            if (!share) { item["id"]=p.id; item["character"]=p.character; }
            for (auto v:p.points) item["points"].push_back({v.x,v.y,v.z});
            return item;
        }
        RaidPath Decode(const json& item,const std::string* recipient=nullptr)
        {
            RaidPath p;
            if (!item.is_object() || !item.at("map").is_number_unsigned() || !item.at("color").is_number_unsigned() ||
                item.at("map").get<uint64_t>()>UINT32_MAX || item.at("color").get<uint64_t>()>UINT32_MAX)
                throw std::runtime_error(Tr("Invalid route data."));
            p.id=recipient?Id():item.at("id").get<std::string>();
            p.character=recipient?*recipient:item.at("character").get<std::string>();
            p.mapId=item.at("map").get<uint32_t>(); p.name=item.at("name").get<std::string>();
            p.color=item.at("color").get<uint32_t>()|IM_COL32_A_MASK;
            p.visible=item.value("visible",true);
            const auto& points=item.at("points");
            if (!p.mapId || !ValidLabel(p.id,256,false) || !ValidLabel(p.character,256,false) ||
                !ValidLabel(p.name,127) || !points.is_array() || points.size()<2 || points.size()>MaxPoints)
                throw std::runtime_error(Tr("Invalid route data."));
            for (const auto& point:points)
            {
                if (!point.is_array() || point.size()!=3 || !point[0].is_number() || !point[1].is_number() || !point[2].is_number())
                    throw std::runtime_error(Tr("Invalid route data."));
                WorldVec3 v{point[0].get<float>(),point[1].get<float>(),point[2].get<float>()};
                if (!Valid(v) || (!p.points.empty() && Distance(p.points.back(),v)>30.0f))
                    throw std::runtime_error(Tr("Invalid route data."));
                p.points.push_back(v);
            }
            return p;
        }
        bool Tool(const char* id,int kind,bool active,float scale)
        {
            ImGui::PushID(id);
            const auto at=ImGui::GetCursorScreenPos();
            const bool clicked=ImGui::InvisibleButton("tool",{30*scale,30*scale});
            auto* d=ImGui::GetWindowDrawList();
            d->AddRectFilled(at,{at.x+30*scale,at.y+30*scale},active?IM_COL32(110,75,23,230):IM_COL32(31,39,43,200),4*scale);
            const auto pt=[&](float x,float y){return ImVec2{at.x+x*scale,at.y+y*scale};};
            const auto gold=IM_COL32(239,202,105,255);
            if (kind==0) // crossed swords
            { d->AddLine(pt(8,7),pt(23,24),gold,3*scale); d->AddLine(pt(22,7),pt(7,24),gold,3*scale); d->AddLine(pt(5,17),pt(13,24),gold,2*scale); d->AddLine(pt(17,24),pt(25,17),gold,2*scale); }
            else if (kind==1) // pencil
            { d->AddLine(pt(8,22),pt(23,7),gold,6*scale); d->AddTriangleFilled(pt(5,25),pt(7,18),pt(12,23),gold); }
            else if (kind==2) d->AddRectFilled(pt(8,8),pt(22,22),IM_COL32(239,100,84,255),2*scale);
            else { d->AddQuadFilled(pt(6,19),pt(16,7),pt(25,15),pt(16,25),gold); d->AddLine(pt(9,16),pt(19,23),IM_COL32(40,40,40,255),2*scale); }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s",Tr(id));
            ImGui::PopID();
            return clicked;
        }
    }
    RaidPath* RaidMode::Find(const std::string& id)
    { for (auto& p:paths) if (p.id==id) return &p; return nullptr; }
    void RaidMode::Select(const RaidPath& p)
    {
        selectedId_=p.id; name_.fill(0); std::copy(p.name.begin(),p.name.end(),name_.begin());
        const auto c=ImGui::ColorConvertU32ToFloat4(p.color); color_={c.x,c.y,c.z};
    }
    bool RaidMode::Button(float scale)
    {
        if (!Tool("Raid mode",0,enabled,scale)) return false;
        enabled=!enabled; windowVisible=enabled;
        if (!enabled) { Finish(); eraser=false; }
        return true;
    }
    void RaidMode::ToggleWindow()
    { if (!enabled) { enabled=true; windowVisible=true; } else windowVisible=!windowVisible; if (!windowVisible) eraser=false; }
    bool RaidMode::Start(const RaidContext& c)
    {
        if (!c.valid || !c.writable || c.mapOpen || !Valid(c.avatar)) return false;
        if (paths.size()>=MaxPaths || PointCount(paths)>=MaxTotalPoints)
        { message_=Tr("Route limit reached. Remove a route before recording."); return false; }
        RaidPath p; p.id=Id(); p.character=c.character; p.mapId=c.mapId;
        p.name=std::string(Tr("Route"))+" "+std::to_string(paths.size()+1);
        p.color=ImGui::ColorConvertFloat4ToU32({color_[0],color_[1],color_[2],1});
        p.points.push_back(c.avatar); recordingId_=p.id; Select(p); paths.push_back(std::move(p));
        lastSample_=ImGui::GetTime(); lastAvatar_=c.avatar; haveAvatar_=true; eraser=false; message_.clear();
        return true;
    }
    bool RaidMode::Finish()
    {
        if (recordingId_.empty()) return false;
        if (auto* p=Find(recordingId_);p && haveAvatar_ && Valid(lastAvatar_) && !p->points.empty() &&
            Distance(p->points.back(),lastAvatar_)>.01f && Distance(p->points.back(),lastAvatar_)<=25.0f &&
            p->points.size()<MaxPoints && PointCount(paths)<MaxTotalPoints) p->points.push_back(lastAvatar_);
        const auto id=recordingId_; recordingId_.clear(); haveAvatar_=false;
        paths.erase(std::remove_if(paths.begin(),paths.end(),[&](const auto& p){return p.id==id && p.points.size()<2;}),paths.end());
        return true;
    }
    bool RaidMode::Update(const RaidContext& c)
    {
        if (contextCharacter_!=c.character || contextMap_!=c.mapId)
        {
            contextCharacter_=c.character; contextMap_=c.mapId;
            deleteMarker_.clear(); deletePath_.clear(); selectedId_.clear(); eraser=false;
        }
        if (!Recording()) return false;
        auto* p=Find(recordingId_);
        if (!p || !enabled || !c.valid || !c.writable || c.character!=p->character || c.mapId!=p->mapId || !Valid(c.avatar)) return Finish();
        if (haveAvatar_ && Distance(lastAvatar_,c.avatar)>25.0f)
        { message_=Tr("Recording stopped after a position jump."); return Finish(); }
        lastAvatar_=c.avatar; haveAvatar_=true;
        const double now=ImGui::GetTime();
        if (now-lastSample_<.12 || Distance(p->points.back(),c.avatar)<.45f) return false;
        if (Distance(p->points.back(),c.avatar)>25.0f || p->points.size()>=MaxPoints || PointCount(paths)>=MaxTotalPoints)
        { message_=Tr("Recording stopped: route limit or position jump."); return Finish(); }
        p->points.push_back(c.avatar); lastSample_=now; return true;
    }
    void RaidMode::SelectForDeletion(std::string pathId,std::string markerId)
    { if (!eraser) return; deletePath_=std::move(pathId); deleteMarker_=std::move(markerId); windowVisible=true; }
    bool RaidMode::Draw(const RaidContext& c,const std::function<void(Icon)>& place,
        const std::function<void(ImDrawList*,ImVec2,float,Icon)>& icon,const std::function<void(const std::string&)>& removeMarker)
    {
        if (!enabled || !windowVisible || !c.valid || c.mapOpen) return false;
        bool changed=false;
        const float s=std::clamp(c.scale,.5f,4.f); const auto screen=ImGui::GetIO().DisplaySize;
        AtlasTheme theme(s);
        ImGui::SetNextWindowSize({std::min(330*s,std::max(100.f,screen.x-16)),0},ImGuiCond_Appearing);
        ImGui::SetNextWindowSizeConstraints({std::min(280*s,std::max(100.f,screen.x-16)),0},{std::max(100.f,screen.x-16),std::max(100.f,screen.y-16)});
        ImGui::SetNextWindowPos({std::max(8.f,screen.x*.5f-165*s),std::max(8.f,screen.y*.15f)},ImGuiCond_Appearing);
        const std::string title=std::string(Tr("Raid tools"))+"###AtlasRaidTools";
        const bool open=ImGui::Begin(title.c_str(),&windowVisible,ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoSavedSettings);
        EndWindow window;
        if (!windowVisible) eraser=false;
        if (open)
        {
            ImGui::TextDisabled("Ctrl+Shift+N");
            if (!c.writable) ImGui::TextWrapped(Tr("Saving blocked by file error"));
            if (Tool(Recording()?"Finish route":"Record route",Recording()?2:1,Recording(),s))
                changed|=Recording()?Finish():Start(c);
            ImGui::SameLine();
            if (Tool("Eraser",3,eraser,s)) { changed|=Finish(); eraser=!eraser; deletePath_.clear(); deleteMarker_.clear(); }
            ImGui::SameLine(); ImGui::TextUnformatted(Tr(Recording()?"Recording your movement":"Ready"));
            if (eraser) ImGui::TextWrapped(Tr("Click a route or marker in the world to delete it."));
            constexpr Icon choices[]{Icon::House,Icon::Swords,Icon::Pickaxe,Icon::Mission,Icon::Star,Icon::Chest,Icon::Flag,Icon::Check,
                Icon::RaidSupport,Icon::RaidDamage,Icon::RaidTank,Icon::RaidHeal};
            for (int i=0;i<12;++i)
            {
                if (i%6) ImGui::SameLine(); ImGui::PushID(i);
                auto at=ImGui::GetCursorScreenPos();
                if (ImGui::InvisibleButton("role",{32*s,32*s}) && c.writable) place(choices[i]);
                icon(ImGui::GetWindowDrawList(),{at.x+16*s,at.y+16*s},15*s,choices[i]);
                ImGui::PopID();
            }
            ImGui::TextDisabled(Tr("Click an icon to mark your current position."));
            if (!deletePath_.empty() || !deleteMarker_.empty())
            {
                ImGui::Separator(); ImGui::TextWrapped(Tr(deleteMarker_.empty()?"Delete this route?":"Delete this marker?"));
                if (ImGui::Button(Tr("Yes, delete")) && c.writable)
                {
                    if (!deleteMarker_.empty()) removeMarker(deleteMarker_);
                    else { if (recordingId_==deletePath_) changed|=Finish(); paths.erase(std::remove_if(paths.begin(),paths.end(),[&](const auto& p){return p.id==deletePath_;}),paths.end()); }
                    deletePath_.clear(); deleteMarker_.clear(); changed=true;
                }
                ImGui::SameLine(); if (ImGui::Button(Tr("Cancel"))) { deletePath_.clear(); deleteMarker_.clear(); }
            }
            if (ImGui::CollapsingHeader(Tr("Routes"),ImGuiTreeNodeFlags_DefaultOpen))
            {
                {
                ImGui::BeginChild("routes",{0,110*s},true);
                EndChild child;
                for (auto& p:paths) if (p.character==c.character && p.mapId==c.mapId)
                {
                    ScopedID id(p.id.c_str());
                    bool visible=p.visible;
                    if (ImGui::Checkbox("##visible",&visible) && c.writable) { p.visible=visible; changed=true; }
                    ImGui::SameLine();
                    const auto textAt=ImGui::GetCursorScreenPos();
                    if (ImGui::Selectable("##route",selectedId_==p.id)) Select(p);
                    ImGui::GetWindowDrawList()->AddText(textAt,ImGui::GetColorU32(ImGuiCol_Text),p.name.c_str());
                }
                }
                if (auto* p=Find(selectedId_);p && p->character==c.character && p->mapId==c.mapId)
                {
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText("##route-name",name_.data(),name_.size()) && c.writable) { p->name=name_.data(); changed=true; }
                    if (ImGui::ColorEdit3(Tr("Route color"),color_.data(),ImGuiColorEditFlags_NoInputs) && c.writable)
                    { p->color=ImGui::ColorConvertFloat4ToU32({color_[0],color_[1],color_[2],1}); changed=true; }
                    if (ImGui::Button(Tr("Delete route"))) { deletePath_=p->id; deleteMarker_.clear(); }
                }
                if (ImGui::Button(Tr("Copy routes")))
                {
                    try {
                        json shared=json::array();
                        for (const auto& p:paths) if (p.character==c.character && p.mapId==c.mapId && p.points.size()>1 && p.id!=recordingId_) shared.push_back(Encode(p,true));
                        if (shared.empty()) throw std::runtime_error(Tr("No routes on this map."));
                        const std::string code="VAR1:"+shared.dump(-1,' ',true);
                        std::string error; if (!CopyLinkToClipboard(code,error)) throw std::runtime_error(error);
                        message_=Tr("Routes copied. Share the code with your group.");
                    } catch(const std::exception& e) { message_=e.what(); }
                }
                ImGui::SameLine();
                if (ImGui::Button(Tr("Paste routes")) && c.writable)
                {
                    try {
                        std::string code,error; if (!ReadLinkFromClipboard(code,error)) throw std::runtime_error(error);
                        if (!code.starts_with("VAR1:") || code.size()>1024*1024) throw std::runtime_error(Tr("Invalid route code."));
                        const auto shared=json::parse(code.substr(5),[](int depth,json::parse_event_t,json&) {
                            if (depth>8) throw std::runtime_error(Tr("Invalid route code.")); return true; });
                        if (!shared.is_array() || shared.empty() || shared.size()+paths.size()>MaxPaths) throw std::runtime_error(Tr("Invalid route code."));
                        std::vector<RaidPath> imported; size_t count=PointCount(paths);
                        for (const auto& item:shared) { auto p=Decode(item,&c.character); count+=p.points.size(); if (count>MaxTotalPoints) throw std::runtime_error(Tr("Route limit reached. Remove a route before recording.")); imported.push_back(std::move(p)); }
                        paths.insert(paths.end(),std::make_move_iterator(imported.begin()),std::make_move_iterator(imported.end())); changed=true;
                        message_=Tr("Routes imported. They appear on their original map.");
                    } catch(const std::exception& e) { message_=e.what(); }
                }
            }
            if (!message_.empty()) ImGui::TextWrapped("%s",message_.c_str());
            const auto size=ImGui::GetWindowSize(),pos=ImGui::GetWindowPos();
            ImGui::SetWindowPos({std::clamp(pos.x,8.f,std::max(8.f,screen.x-size.x-8)),std::clamp(pos.y,8.f,std::max(8.f,screen.y-size.y-8))});
        }
        return changed;
    }
    nlohmann::json RaidMode::Save() const
    {
        json data{{"enabled",enabled},{"paths",json::array()}};
        for (const auto& p:paths) if (p.points.size()>1) data["paths"].push_back(Encode(p,false));
        return data;
    }
    void RaidMode::Load(const nlohmann::json& data)
    {
        Reset(); if (data.is_null()) return;
        const auto& list=data.at("paths");
        if (!list.is_array() || list.size()>MaxPaths) throw std::runtime_error(Tr("Invalid route data."));
        std::vector<RaidPath> loaded; std::unordered_set<std::string> ids; size_t count=0;
        for (const auto& item:list) { auto p=Decode(item); count+=p.points.size(); if (!ids.insert(p.id).second || count>MaxTotalPoints) throw std::runtime_error(Tr("Invalid route data.")); loaded.push_back(std::move(p)); }
        enabled=data.value("enabled",false); paths=std::move(loaded);
    }
    void RaidMode::Reset()
    { *this=RaidMode{}; }
}
