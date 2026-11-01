#include "RaidMode.h"
#include "RoutePatterns.h"
#include "AtlasClipboard.h"
#include "AtlasLocalization.h"
#include "AtlasTheme.h"
#include "MarkerLink.h"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <stdexcept>
#include <sstream>
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
            json item{{"map",p.mapId},{"name",p.name},{"color",p.color},{"visible",p.visible},{"style",static_cast<int>(p.style)},{"points",json::array()}};
            if (!share) { item["id"]=p.id; item["character"]=p.character; item["folder_id"]=p.folderId; }
            for (auto v:p.points) item["points"].push_back({v.x,v.y,v.z});
            if (p.mapAnchor) item["map_anchor"]={{"world",{p.mapAnchor->world.x,p.mapAnchor->world.y,p.mapAnchor->world.z}},
                {"continent",{p.mapAnchor->continent.x,p.mapAnchor->continent.y}}};
            return item;
        }
        RaidPath Decode(const json& item,const std::string* recipient=nullptr)
        {
            RaidPath p;
            if(item.contains("style")) {
                if(!item["style"].is_number_integer())throw std::runtime_error(Tr("Invalid route data."));
                const int style=item["style"].get<int>();
                if(style<0 || style>3)throw std::runtime_error(Tr("Invalid route data."));
                p.style=static_cast<PathStyle>(style);
            }
            if (!item.is_object() || !item.at("map").is_number_unsigned() || !item.at("color").is_number_unsigned() ||
                item.at("map").get<uint64_t>()>UINT32_MAX || item.at("color").get<uint64_t>()>UINT32_MAX)
                throw std::runtime_error(Tr("Invalid route data."));
            p.id=recipient?Id():item.at("id").get<std::string>();
            p.character=recipient?*recipient:item.at("character").get<std::string>();
            p.mapId=item.at("map").get<uint32_t>(); p.name=item.at("name").get<std::string>();
            p.color=item.at("color").get<uint32_t>()|IM_COL32_A_MASK;
            p.visible=item.value("visible",true);
            if (!recipient) p.folderId=item.value("folder_id",std::string{});
            if (!ValidLabel(p.folderId,256)) throw std::runtime_error(Tr("Invalid route data."));
            if (item.contains("map_anchor"))
            {
                const auto& a=item.at("map_anchor");
                const auto& w=a.at("world"); const auto& m=a.at("continent");
                if (!w.is_array() || w.size()!=3 || !m.is_array() || m.size()!=2)
                    throw std::runtime_error(Tr("Invalid route data."));
                RaidMapAnchor anchor{{w[0].get<float>(),w[1].get<float>(),w[2].get<float>()},{m[0].get<float>(),m[1].get<float>()}};
                if (!Valid(anchor.world) || !std::isfinite(anchor.continent.x) || !std::isfinite(anchor.continent.y) ||
                    std::abs(anchor.continent.x)>10000000 || std::abs(anchor.continent.y)>10000000)
                    throw std::runtime_error(Tr("Invalid route data."));
                p.mapAnchor=anchor;
            }
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
        selectedId_=p.id; newStyle_=p.style; name_.fill(0); std::copy(p.name.begin(),p.name.end(),name_.begin());
        const auto c=ImGui::ColorConvertU32ToFloat4(p.color); color_={c.x,c.y,c.z};
    }
    bool RaidMode::Button(float scale)
    {
        if (!Tool("Pathing mode",1,enabled,scale)) return false;
        enabled=!enabled; windowVisible=enabled;
        if (!enabled) { Finish("pathing disabled"); eraser=false; }
        return true;
    }
    void RaidMode::ToggleWindow()
    { if (!enabled) { enabled=true; windowVisible=true; } else windowVisible=!windowVisible; if (!windowVisible) eraser=false; }
    bool RaidMode::SelectProject(const std::string& folderId)
    {
        if (activeFolder==folderId) return false;
        const bool changed=Finish("project changed");
        if (diagnosticLog_) diagnosticLog_(std::string("project selected folder=")+folderId);
        activeFolder=folderId;
        selectedId_.clear(); deletePath_.clear(); deleteMarker_.clear(); message_.clear();
        name_.fill(0); search_.fill(0); eraser=false;
        return changed;
    }
    bool RaidMode::Start(const RaidContext& c)
    {
        diagnosticLog_=c.diagnostics;
        if (!c.valid || !c.writable || c.mapOpen || !Valid(c.avatar)) {
            if (diagnosticLog_) diagnosticLog_("start rejected: invalid context");
            return false;
        }
        if (activeFolder.empty()) {message_=Tr("Select or create a project first."); if (diagnosticLog_) diagnosticLog_("start rejected: no project"); return false;}
        if (paths.size()>=MaxPaths || PointCount(paths)>=MaxTotalPoints)
        { message_=Tr("Route limit reached. Remove a route before recording."); return false; }
        RaidPath p; p.id=Id(); p.character=c.character; p.mapId=c.mapId;
        p.mapAnchor=c.mapAnchor;
        p.folderId=activeFolder; p.style=newStyle_;
        p.name=name_[0] ? std::string(name_.data()) : std::string(Tr("Route"))+" "+std::to_string(paths.size()+1);
        p.color=ImGui::ColorConvertFloat4ToU32({color_[0],color_[1],color_[2],1});
        p.points.push_back(c.avatar); recordingId_=p.id; Select(p); paths.push_back(std::move(p));
        lastSample_=ImGui::GetTime(); invalidSince_=-1; lastAvatar_=c.avatar; haveAvatar_=true; eraser=false; message_.clear();
        if (diagnosticLog_) { std::ostringstream out; out<<"started id="<<recordingId_<<" map="<<c.mapId<<" folder="<<activeFolder<<" points=1"; diagnosticLog_(out.str()); }
        return true;
    }
    bool RaidMode::Finish(std::string_view reason)
    {
        if (recordingId_.empty()) return false;
        const auto id=recordingId_;
        if (auto* p=Find(recordingId_);p && haveAvatar_ && Valid(lastAvatar_) && !p->points.empty() &&
            Distance(p->points.back(),lastAvatar_)>.01f && Distance(p->points.back(),lastAvatar_)<=25.0f &&
            p->points.size()<MaxPoints && PointCount(paths)<MaxTotalPoints) p->points.push_back(lastAvatar_);
        recordingId_.clear(); haveAvatar_=false; invalidSince_=-1;
        paths.erase(std::remove_if(paths.begin(),paths.end(),[&](const auto& p){return p.id==id && p.points.size()<2;}),paths.end());
        if (diagnosticLog_) {
            const auto* saved=Find(id);
            std::ostringstream out;
            out<<"closed id="<<id<<" reason="<<reason<<" points="<<(saved?saved->points.size():0);
            if (!saved) out<<" discarded=true";
            diagnosticLog_(out.str());
        }
        return true;
    }
    bool RaidMode::Update(const RaidContext& c)
    {
        diagnosticLog_=c.diagnostics;
        if (c.valid && (contextCharacter_!=c.character || contextMap_!=c.mapId))
        {
            if (!contextCharacter_.empty() && diagnosticLog_) { std::ostringstream out; out<<"context changed from character="<<contextCharacter_<<" map="<<contextMap_<<" to character="<<c.character<<" map="<<c.mapId; diagnosticLog_(out.str()); }
            contextCharacter_=c.character; contextMap_=c.mapId;
            deleteMarker_.clear(); deletePath_.clear(); selectedId_.clear(); eraser=false;
        }
        if (!Recording()) return false;
        auto* p=Find(recordingId_);
        if (!p) return Finish("recording route missing");
        if (!enabled) return Finish("pathing disabled during recording");
        if (!c.writable) return Finish("storage became read-only");
        const double now=ImGui::GetTime();
        // A torn MumbleLink snapshot can affect a single frame. Never sample it.
        // Stop after a sustained gap so loading cannot join unrelated positions.
        if (!c.valid || !Valid(c.avatar)) {
            if (invalidSince_<0) {
                invalidSince_=now;
                if (diagnosticLog_) diagnosticLog_("recording paused: gameplay data unavailable");
            }
            if (now-invalidSince_>=.5) {
                message_=Tr("Recording stopped: gameplay data unavailable.");
                return Finish("gameplay data unavailable for 500ms");
            }
            return false;
        }
        if (invalidSince_>=0) {
            if (now-invalidSince_>=.5) {
                message_=Tr("Recording stopped: gameplay data unavailable.");
                return Finish("gameplay data gap exceeded 500ms");
            }
            invalidSince_=-1;
            if (diagnosticLog_) diagnosticLog_("recording resumed after brief data gap");
        }
        if (c.character!=p->character || c.mapId!=p->mapId) return Finish("character or map changed");
        if (haveAvatar_ && Distance(lastAvatar_,c.avatar)>25.0f)
        { message_=Tr("Recording stopped after a position jump."); return Finish("avatar jump over 25m"); }
        lastAvatar_=c.avatar; haveAvatar_=true;
        if (now-lastSample_<.12 || Distance(p->points.back(),c.avatar)<.45f) return false;
        if (Distance(p->points.back(),c.avatar)>25.0f || p->points.size()>=MaxPoints || PointCount(paths)>=MaxTotalPoints)
        { message_=Tr("Recording stopped: route limit or position jump."); return Finish("route limit or position jump"); }
        p->points.push_back(c.avatar); lastSample_=now; return true;
    }
    void RaidMode::SelectForDeletion(std::string pathId,std::string markerId)
    { if (!eraser || (pathId.empty() && markerId.empty()) || !deletePath_.empty() || !deleteMarker_.empty()) return; deletePath_=std::move(pathId); deleteMarker_=std::move(markerId); windowVisible=true; }
    bool RaidMode::Draw(const RaidContext& c,const std::function<void(Icon)>& place,
        const std::function<void(ImDrawList*,ImVec2,float,Icon)>& icon,const std::function<void(const std::string&)>& removeMarker,
        const std::function<void(int)>& collections)
    {
        diagnosticLog_=c.diagnostics;
        if (!enabled || !windowVisible || !c.valid || c.mapOpen) return false;
        bool changed=false;
        const float s=AtlasUiScale(c.scale); const auto screen=ImGui::GetIO().DisplaySize;
        AtlasTheme theme(s);
        const float maxW=std::max(100.f,screen.x-16),maxH=std::max(100.f,screen.y-16);
        ImGui::SetNextWindowSize({std::min(640*s,maxW),std::min(560*s,maxH)},ImGuiCond_Appearing);
        ImGui::SetNextWindowSizeConstraints({std::min(580*s,maxW),std::min(420*s,maxH)},{maxW,maxH});
        ImGui::SetNextWindowPos({std::max(8.f,screen.x*.5f-320*s),std::max(8.f,screen.y*.12f)},ImGuiCond_Appearing);
        const std::string title=std::string(Tr("Pathing"))+"###AtlasRaidTools";
        const bool open=ImGui::Begin(title.c_str(),&windowVisible,ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoTitleBar);
        EndWindow window;
        ImGui::SetWindowFontScale(.90f);
        if (!windowVisible) eraser=false;
        if (open)
        {
            if(!AtlasWindowHeader(Tr("Pathing"),windowVisible,s,AtlasGlyph::Pencil)){eraser=false;return changed;}
            collections(0);
            constexpr const char* tabs[]{"Record","Projects","Markers","Settings"};
            constexpr AtlasGlyph glyphs[]{AtlasGlyph::Pencil,AtlasGlyph::Route,AtlasGlyph::Pin,AtlasGlyph::Settings};
            const float nav=(ImGui::GetContentRegionAvail().x-ImGui::GetStyle().ItemSpacing.x*3)/4;
            for(int i=0;i<4;++i) {
                if(i) ImGui::SameLine(); ImGui::PushID(i);
                if(AtlasTile("tab",Tr(tabs[i]),glyphs[i],{nav,30*s},tab_==i)) tab_=i;
                ImGui::PopID();
            }
            ImGui::Spacing();
            if (!c.writable) ImGui::TextWrapped(Tr("Saving blocked by file error"));
            if(Recording()) ImGui::TextColored({1,.65f,.35f,1},"%s",Tr("Recording your movement"));
            if(eraser) ImGui::TextColored({1,.8f,.4f,1},"%s",Tr("Eraser active: click a highlighted path or marker. Esc to cancel."));
            const auto current=[&]() -> RaidPath* {
                auto* p=Find(selectedId_);
                return p && p->character==c.character && p->mapId==c.mapId && p->folderId==activeFolder ? p : nullptr;
            };
            const auto palette=[&] {
                ImGui::TextUnformatted(Tr("Path color"));
                constexpr ImU32 colors[]{IM_COL32(245,59,72,255),IM_COL32(255,144,35,255),IM_COL32(249,208,53,255),IM_COL32(48,220,111,255),
                    IM_COL32(45,195,242,255),IM_COL32(166,91,244,255),IM_COL32(236,81,205,255),IM_COL32(240,243,255,255)};
                constexpr const char* names[]{"Red","Orange","Gold","Green","Cyan","Purple","Pink","White"};
                const float side=std::min(26*s,(ImGui::GetContentRegionAvail().x-7*5*s)/8);
                for(int i=0;i<8;++i) {
                    if(i) ImGui::SameLine(0,5*s); ImGui::PushID(i);
                    const auto at=ImGui::GetCursorScreenPos(); const auto col=ImGui::ColorConvertU32ToFloat4(colors[i]);
                    if(ImGui::InvisibleButton("color",{side,side})) {
                        color_={col.x,col.y,col.z};
                        if(auto* p=current();p && c.writable) {p->color=colors[i];changed=true;}
                    }
                    auto* d=ImGui::GetWindowDrawList(); const ImVec2 center{at.x+side*.5f,at.y+side*.5f};
                    d->AddCircleFilled(center,side*.38f,colors[i],24);
                    if(std::abs(color_[0]-col.x)+std::abs(color_[1]-col.y)+std::abs(color_[2]-col.z)<.01f)
                    { d->AddCircle(center,side*.47f,IM_COL32(80,185,245,70),28,5*s); d->AddCircle(center,side*.46f,IM_COL32_WHITE,28,1.6f*s); }
                    if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s",Tr(names[i])); ImGui::PopID();
                }
            };
            const auto preview=[&] {
                ImGui::Spacing(); ImGui::TextUnformatted(Tr("Route preview"));
                const auto at=ImGui::GetCursorScreenPos(); const ImVec2 size{ImGui::GetContentRegionAvail().x,70*s};
                ImGui::InvisibleButton("preview",size); auto* d=ImGui::GetWindowDrawList();
                const ImVec2 end{at.x+size.x,at.y+size.y}; d->AddRectFilled(at,end,IM_COL32(7,19,34,255),5*s);
                d->PushClipRect(at,end,true);
                for(float x=at.x+16*s;x<end.x;x+=24*s) d->AddLine({x,at.y},{x,end.y},IM_COL32(52,78,104,65));
                for(float y=at.y+16*s;y<end.y;y+=24*s) d->AddLine({at.x,y},{end.x,y},IM_COL32(52,78,104,65));
                if(const auto* p=current();p && p->points.size()>1) {
                    float minX=p->points[0].x,maxX=minX,minZ=p->points[0].z,maxZ=minZ;
                    for(const auto& v:p->points){minX=std::min(minX,v.x);maxX=std::max(maxX,v.x);minZ=std::min(minZ,v.z);maxZ=std::max(maxZ,v.z);}
                    const float zoom=std::min((size.x-32*s)/std::max(.01f,maxX-minX),(size.y-28*s)/std::max(.01f,maxZ-minZ));
                    const auto project=[&](WorldVec3 v){return ImVec2{at.x+size.x*.5f+(v.x-(minX+maxX)*.5f)*zoom,at.y+size.y*.5f-(v.z-(minZ+maxZ)*.5f)*zoom};};
                    const size_t step=std::max(size_t{1},p->points.size()/160);
                    const float arrowLength=4*s;
                    auto arrows=MakePathArrowLayout(p->style,3*s,appearance.arrows,arrowLength,
                        (p->style==PathStyle::Arrows?14.f:40.f)*s);
                    double totalLength=0;
                    for(size_t i=step;i<p->points.size()+step;i+=step) {
                        const auto a=project(p->points[i-step]),b=project(p->points[std::min(i,p->points.size()-1)]);
                        const float dx=b.x-a.x,dy=b.y-a.y;totalLength+=std::sqrt(dx*dx+dy*dy);
                    }
                    arrows.endCut=std::max(0.,totalLength-arrowLength*PathEndArrowScale*1.5);
                    auto last=project(p->points.front());double arc=0;
                    for(size_t i=step;i<p->points.size()+step;i+=step){auto next=project(p->points[std::min(i,p->points.size()-1)]);
                        const float dx=next.x-last.x,dy=next.y-last.y,len=std::sqrt(dx*dx+dy*dy);
                        const auto sample=[&](float t){return ImVec2{last.x+dx*t,last.y+dy*t};};
                        VisitPathPattern(p->style,arc,len,3*s,[&](float from,float to){
                            d->AddLine(sample(from),sample(to),(p->color&0xffffff)|IM_COL32(0,0,0,45),6*s);d->AddLine(sample(from),sample(to),p->color,2*s);
                        },[&](float t){d->AddCircleFilled(sample(t),2*s,p->color,12);},arrows);
                        VisitPathArrows(arrows,arc,len,[&](float t){
                            const auto center=sample(t);const float ux=dx/len,uy=dy/len,r=arrowLength;
                            const ImVec2 tip{center.x+ux*r*.5f,center.y+uy*r*.5f};
                            d->AddTriangleFilled(tip,{tip.x-ux*r+uy*r*.45f,tip.y-uy*r-ux*r*.45f},{tip.x-ux*r-uy*r*.45f,tip.y-uy*r+ux*r*.45f},p->color);
                        });
                        arc+=len;last=next;
                    }
                    const auto start=project(p->points.front()),tip=project(p->points.back());
                    d->AddCircleFilled(start,7*s,(p->color&0xffffff)|IM_COL32(0,0,0,55),20);
                    d->AddCircleFilled(start,3*s,IM_COL32_WHITE);
                    ImVec2 direction{0,-1};
                    for(size_t i=p->points.size()-1;i>0;--i){const auto before=project(p->points[i-1]);
                        const float dx=tip.x-before.x,dy=tip.y-before.y,length=std::sqrt(dx*dx+dy*dy);
                        if(length>.01f){direction={dx/length,dy/length};break;}}
                    const float r=arrowLength*PathEndArrowScale;
                    d->AddTriangleFilled(tip,{tip.x-direction.x*r+direction.y*r*.6f,tip.y-direction.y*r-direction.x*r*.6f},
                        {tip.x-direction.x*r-direction.y*r*.6f,tip.y-direction.y*r+direction.x*r*.6f},p->color);
                } else d->AddText({at.x+12*s,at.y+size.y*.5f-8*s},IM_COL32(151,171,200,255),Tr("Record a path to see its preview."));
                d->PopClipRect();
            };
            const auto routeList=[&] {
                AtlasHeading(Tr("Paths in this project"));
                ImGui::SetNextItemWidth(std::max(40*s,ImGui::GetContentRegionAvail().x-48*s));
                ImGui::InputTextWithHint("##search",Tr("Search paths..."),search_.data(),search_.size());
                ImGui::SameLine();if(ImGui::Button(sortAscending_?"A-Z":"Z-A",{42*s,0}))sortAscending_=!sortAscending_;
                std::vector<RaidPath*> ordered;
                for(auto& path:paths)if(path.character==c.character && path.mapId==c.mapId && path.folderId==activeFolder)ordered.push_back(&path);
                std::stable_sort(ordered.begin(),ordered.end(),[&](const auto* a,const auto* b){return sortAscending_?a->name<b->name:b->name<a->name;});
                size_t found=0; bool focusName=false;
                {
                    ImGui::BeginChild("path-list",{0,std::max(100*s,ImGui::GetContentRegionAvail().y-190*s)},true);
                    EndChild child;
                    for(auto* entry:ordered) {
                        auto& p=*entry;
                        std::string hay=p.name,needle=search_.data();
                        const auto lower=[](unsigned char ch){return static_cast<char>(std::tolower(ch));};
                        std::transform(hay.begin(),hay.end(),hay.begin(),lower); std::transform(needle.begin(),needle.end(),needle.begin(),lower);
                        if(!needle.empty() && hay.find(needle)==std::string::npos) continue; ++found;
                        ScopedID id(p.id.c_str()); const float row=40*s,controls=88*s;
                        const auto at=ImGui::GetCursorScreenPos();
                        const float w=std::max(30*s,ImGui::GetContentRegionAvail().x-controls);
                        if(ImGui::Selectable("##path",selectedId_==p.id,0,{w,row})) Select(p);
                        auto* d=ImGui::GetWindowDrawList(); DrawAtlasGlyph(d,{at.x+13*s,at.y+17*s},8*s,AtlasGlyph::Route,p.color);
                        d->PushClipRect({at.x+30*s,at.y},{at.x+w,at.y+row},true);
                        d->AddText({at.x+29*s,at.y+4*s},IM_COL32(228,235,251,255),p.name.c_str());
                        const auto count=std::to_string(p.points.size())+" "+Tr("samples");
                        d->AddText({at.x+32*s,at.y+23*s},IM_COL32(146,166,194,255),count.c_str()); d->PopClipRect();
                        ImGui::SameLine(); if(EyeButton("visibility",p.visible,s) && c.writable){p.visible=!p.visible;changed=true;}
                        ImGui::SameLine(); if(AtlasTile("edit","",AtlasGlyph::Pencil,{20*s,22*s})) {Select(p);focusName=true;}
                        if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",Tr("Edit path"));
                        ImGui::SameLine(); if(AtlasTile("delete","",AtlasGlyph::Trash,{22*s,22*s},false,true)) {deletePath_=p.id;deleteMarker_.clear();}
                        ImGui::Separator();
                    }
                    if(!found) ImGui::TextWrapped(Tr("No paths here yet. Open Record and record your movement."));
                }
                if(auto* p=current();p && ImGui::CollapsingHeader(Tr("Route details"),ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::TextDisabled(Tr("Points: %zu"),p->points.size());
                    ImGui::TextUnformatted(Tr("Path name")); ImGui::SetNextItemWidth(-1);
                    if(focusName)ImGui::SetKeyboardFocusHere();
                    if(ImGui::InputText("##path-name",name_.data(),name_.size()) && c.writable){p->name=name_.data();changed=true;}
                    float length=0; for(size_t i=1;i<p->points.size();++i) length+=Distance(p->points[i-1],p->points[i]);
                    ImGui::TextDisabled(Tr("Length: %.0f units"),MetresToWorldUnits(length));
                }
                if(ImGui::CollapsingHeader(Tr("Display options"),ImGuiTreeNodeFlags_DefaultOpen)) {
                    changed|=ImGui::Checkbox(Tr("Routes in 3D"),&appearance.world);
                    changed|=ImGui::Checkbox(Tr("Routes on map"),&appearance.map);
                    changed|=ImGui::Checkbox(Tr("Routes on minimap"),&appearance.minimap);
                    changed|=ImGui::Checkbox(Tr("Direction arrows"),&appearance.arrows);
                }
            };
            const auto copyPaths=[&] {
                if(ImGui::Button(Tr("Copy routes"))) try {
                    json shared=json::array();
                    for(const auto& p:paths) if(p.character==c.character && p.mapId==c.mapId && p.folderId==activeFolder && p.points.size()>1 && p.id!=recordingId_) shared.push_back(Encode(p,true));
                    if(shared.empty()) throw std::runtime_error(Tr("No routes on this map."));
                    std::string error; if(!CopyLinkToClipboard("VAR1:"+shared.dump(-1,' ',true),error)) throw std::runtime_error(error);
                    message_=Tr("Routes copied. Share the code with your group.");
                } catch(const std::exception& e){message_=e.what();}
                if(ImGui::Button(Tr("Paste routes")) && c.writable) try {
                    std::string code,error; if(!ReadLinkFromClipboard(code,error)) throw std::runtime_error(error);
                    if(!code.starts_with("VAR1:")) throw std::runtime_error(Tr("Invalid route code."));
                    auto shared=json::parse(code.substr(5),[](int depth,json::parse_event_t,json&){if(depth>8) throw std::runtime_error(Tr("Invalid route code."));return true;});
                    auto imported=DecodeSharedPaths(shared,c.character);
                    if(activeFolder.empty() || paths.size()+imported.size()>MaxPaths || PointCount(paths)+PointCount(imported)>MaxTotalPoints)
                        throw std::runtime_error(Tr("Route limit reached. Remove a route before recording."));
                    for(auto& p:imported)p.folderId=activeFolder;
                    paths.insert(paths.end(),std::make_move_iterator(imported.begin()),std::make_move_iterator(imported.end()));changed=true;
                    message_=Tr("Routes imported. They appear on their original map.");
                } catch(const std::exception& e){message_=e.what();}
            };
            {
                ImGui::BeginChild("workspace",{0,-58*s},false); EndChild workspace;
                const bool split=ImGui::GetContentRegionAvail().x>510*s && tab_<2;
                const float total=ImGui::GetContentRegionAvail().x;
                if(split) ImGui::BeginChild("actions",{total*.55f,0},true);
                if(tab_==0) {
                    AtlasHeading(Tr("Tool"));
                    const float toolWidth=(ImGui::GetContentRegionAvail().x-ImGui::GetStyle().ItemSpacing.x)*.5f;
                    AtlasTile("path-tool",Tr("Route"),AtlasGlyph::Route,{toolWidth,42*s},true,false,true);
                    ImGui::SameLine();
                    if(AtlasTile("marker-tool",Tr("Place a marker"),AtlasGlyph::Pin,{toolWidth,42*s},false,false,true))tab_=2;
                    ImGui::TextUnformatted(Tr("Path name"));ImGui::SetNextItemWidth(-1);
                    if(ImGui::InputTextWithHint("##draw-path-name",Tr("Name your path..."),name_.data(),name_.size()) && c.writable)
                        if(auto* path=current()){path->name=name_.data();changed=true;}
                    ImGui::Spacing();
                    ImGui::TextUnformatted(Tr("Quick actions"));
                    const float unit=(ImGui::GetContentRegionAvail().x-ImGui::GetStyle().ItemSpacing.x*3)/4.8f;
                    if(AtlasTile("record",Tr(Recording()?"Stop":"Record"),Recording()?AtlasGlyph::Stop:AtlasGlyph::Pencil,{unit,42*s},Recording(),false,true) && c.writable)
                        changed|=Recording()?Finish("manual stop"):Start(c);
                    ImGui::SameLine();
                    if(AtlasTile("eraser",Tr("Eraser"),AtlasGlyph::Eraser,{unit,42*s},eraser,false,true) && c.writable)
                    {changed|=Finish("eraser enabled");eraser=!eraser;deletePath_.clear();deleteMarker_.clear();}
                    ImGui::SameLine();
                    if(AtlasTile("copy-drawing",Tr("Copy"),AtlasGlyph::Copy,{unit*1.4f,42*s},false,false,true))collections(3);
                    ImGui::SameLine();if(AtlasTile("paste-drawing",Tr("Paste"),AtlasGlyph::Paste,{unit*1.4f,42*s},false,false,true)){collections(4);tab_=1;}
                    ImGui::Separator(); palette(); ImGui::Spacing();
                    ImGui::TextUnformatted(Tr("Ground beam width")); ImGui::SetNextItemWidth(-1);
                    changed|=ImGui::SliderFloat("##width",&appearance.width,.15f,2.f,"%.2f m",ImGuiSliderFlags_AlwaysClamp);
                    ImGui::TextUnformatted(Tr("Line style"));
                    constexpr const char* styleNames[]{"Continuous","Dashed","Dots","Arrow trail"};
                    const float styleWidth=(ImGui::GetContentRegionAvail().x-3*ImGui::GetStyle().ItemSpacing.x)/4;
                    for(int i=0;i<4;++i) {
                        if(i)ImGui::SameLine();ImGui::PushID(i);
                        const auto at=ImGui::GetCursorScreenPos();
                        if(AtlasTile("style","",AtlasGlyph::None,{styleWidth,26*s},static_cast<int>(newStyle_)==i) && c.writable) {
                            newStyle_=static_cast<PathStyle>(i);
                            if(auto* path=current()){path->style=newStyle_;changed=true;}
                        }
                        auto* d=ImGui::GetWindowDrawList();const float y=at.y+13*s,left=at.x+8*s,right=at.x+styleWidth-8*s;
                        const auto ink=IM_COL32(228,232,245,255);
                        if(i==0)d->AddLine({left,y},{right,y},ink,1.8f*s);
                        if(i==1)for(float x=left;x<right;x+=11*s)d->AddLine({x,y},{std::min(right,x+6*s),y},ink,1.8f*s);
                        if(i==2)for(float x=left+2*s;x<right;x+=8*s)d->AddCircleFilled({x,y},1.8f*s,ink,12);
                        if(i==3)for(float x=left+4*s;x<right;x+=14*s)d->AddTriangleFilled({x+4*s,y},{x-2*s,y-3*s},{x-2*s,y+3*s},ink);
                        if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",Tr(styleNames[i]));ImGui::PopID();
                    }
                    ImGui::Spacing();ImGui::TextUnformatted(Tr("Markers"));
                    constexpr Icon drawingIcons[]{Icon::House,Icon::Swords,Icon::Pickaxe,Icon::Mission,Icon::Star,Icon::Chest,
                        Icon::Flag,Icon::Check,Icon::RaidSupport,Icon::RaidDamage,Icon::RaidTank,Icon::RaidHeal};
                    const float iconSide=std::min(32*s,(ImGui::GetContentRegionAvail().x-5*ImGui::GetStyle().ItemSpacing.x)/6);
                    for(int index=0;index<12;++index){
                        if(index%6)ImGui::SameLine();ImGui::PushID(100+index);const auto at=ImGui::GetCursorScreenPos();
                        if(AtlasTile("place","",AtlasGlyph::None,{iconSide,iconSide}) && c.writable)place(drawingIcons[index]);
                        icon(ImGui::GetWindowDrawList(),{at.x+iconSide*.5f,at.y+iconSide*.5f},iconSide*.4f,drawingIcons[index]);
                        if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",Tr("Click an icon to mark your current position."));ImGui::PopID();
                    }
                    preview();
                } else if(tab_==1) {
                    collections(1);
                    if(ImGui::CollapsingHeader(Tr("Share paths only"))) copyPaths();
                } else if(tab_==2) {
                    AtlasHeading(Tr("Place a marker")); ImGui::TextWrapped(Tr("Click an icon to mark your current position."));
                    constexpr Icon choices[]{Icon::House,Icon::Swords,Icon::Pickaxe,Icon::Mission,Icon::Star,Icon::Chest,Icon::Flag,Icon::Check,
                        Icon::RaidSupport,Icon::RaidDamage,Icon::RaidTank,Icon::RaidHeal};
                    const float side=std::min(44*s,(ImGui::GetContentRegionAvail().x-5*ImGui::GetStyle().ItemSpacing.x)/6);
                    for(int i=0;i<12;++i) {
                        if(i%6) ImGui::SameLine(); ImGui::PushID(i); const auto at=ImGui::GetCursorScreenPos();
                        if(AtlasTile("marker","",AtlasGlyph::None,{side,side}) && c.writable)place(choices[i]);
                        icon(ImGui::GetWindowDrawList(),{at.x+side*.5f,at.y+side*.5f},side*.4f,choices[i]);
                        ImGui::PopID();
                    }
                    ImGui::Spacing(); collections(2);
                } else {AtlasHeading(Tr("Route appearance")); changed|=DrawAppearance();}
                if(split) {ImGui::EndChild(); ImGui::SameLine(); ImGui::BeginChild("browser",{0,0},true); routeList(); ImGui::EndChild();}
                else if(tab_==1) {ImGui::Separator();routeList();}
            }
            const float footer=(ImGui::GetContentRegionAvail().x-2*ImGui::GetStyle().ItemSpacing.x)/3;
            if(GoldButton(Tr("Save changes"),{footer,26*s}) && c.writable)collections(5);
            ImGui::SameLine(); if(ImGui::Button(Tr("Close"),{footer,26*s})){windowVisible=false;eraser=false;}
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(.46f,.15f,.20f,1));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(.60f,.20f,.26f,1));
            if(ImGui::Button(Tr("Delete route"),{footer,26*s}))
                if(auto* path=current()){deletePath_=path->id;deleteMarker_.clear();windowVisible=true;}
            ImGui::PopStyleColor(2);
            if(!message_.empty()) ImGui::TextWrapped("%s",message_.c_str());
            else ImGui::TextDisabled("%s  |  Ctrl+Shift+N",Tr("Changes are saved automatically."));
            if(!deletePath_.empty() || !deleteMarker_.empty()) {
                const std::string popupLabel=std::string(Tr("Confirm deletion"))+"###AtlasDrawingDelete";
                const char* popup=popupLabel.c_str();
                if(!ImGui::IsPopupOpen(popup)) ImGui::OpenPopup(popup);
                ImGui::SetNextWindowPos({screen.x*.5f,screen.y*.5f},ImGuiCond_Appearing,{.5f,.5f});
                SetNextAtlasAutoHeightWindowSize(360*s);
                bool confirm=true;
                if(ImGui::BeginPopupModal(popup,&confirm,ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings)) {
                    ImGui::TextWrapped("%s",Tr(deleteMarker_.empty()?"Delete this route?":"Delete this marker?"));
                    if(auto* p=Find(deletePath_)) ImGui::TextWrapped("%s",p->name.c_str());
                    ImGui::Spacing();
                    if(GoldButton(Tr("Yes, delete"),{-1,36*s}) && c.writable) {
                        if(!deleteMarker_.empty())removeMarker(deleteMarker_);
                        else {if(recordingId_==deletePath_)changed|=Finish("route deleted");paths.erase(std::remove_if(paths.begin(),paths.end(),[&](const auto& p){return p.id==deletePath_ && p.character==c.character;}),paths.end());}
                        changed=true;confirm=false;
                    }
                    if(ImGui::Button(Tr("Cancel"),{-1,32*s}))confirm=false;
                    if(!confirm){deletePath_.clear();deleteMarker_.clear();ImGui::CloseCurrentPopup();}
                    ImGui::EndPopup();
                }
                if(!confirm){deletePath_.clear();deleteMarker_.clear();}
            }
            const auto size=ImGui::GetWindowSize(),pos=ImGui::GetWindowPos();
            ImGui::SetWindowPos({std::clamp(pos.x,8.f,std::max(8.f,screen.x-size.x-8)),std::clamp(pos.y,8.f,std::max(8.f,screen.y-size.y-8))});
        }
        return changed;
    }
    nlohmann::json RaidMode::Save() const
    {
        json data{{"enabled",enabled},{"paths",json::array()}};
        data["appearance"]={{"map",appearance.map},{"minimap",appearance.minimap},{"world",appearance.world},
            {"arrows",appearance.arrows},{"animate",appearance.animate},{"opacity",appearance.opacity},{"width",appearance.width},
            {"map_width",appearance.mapWidth},{"glow",appearance.glow},{"speed",appearance.speed}};
        for (const auto& p:paths) if (p.points.size()>1) data["paths"].push_back(Encode(p,false));
        return data;
    }
    void RaidMode::Load(const nlohmann::json& data)
    {
        Reset(); if (data.is_null()) return;
        if (data.contains("appearance"))
        {
            const auto& a=data.at("appearance");
            appearance.map=a.value("map",true); appearance.minimap=a.value("minimap",true); appearance.world=a.value("world",true);
            appearance.arrows=a.value("arrows",true); appearance.animate=a.value("animate",true);
            const auto number=[&](const char* key,float fallback,float low,float high) {
                float value=a.value(key,fallback); if (!std::isfinite(value) || value<low || value>high) throw std::runtime_error(Tr("Invalid route data.")); return value; };
            appearance.opacity=number("opacity",.85f,.1f,1.f); appearance.width=number("width",.55f,.15f,2.f);
            appearance.mapWidth=number("map_width",3.f,1.f,8.f); appearance.glow=number("glow",.7f,0.f,1.f); appearance.speed=number("speed",1.f,.2f,3.f);
        }
        const auto& list=data.at("paths");
        if (!list.is_array() || list.size()>MaxPaths) throw std::runtime_error(Tr("Invalid route data."));
        std::vector<RaidPath> loaded; std::unordered_set<std::string> ids; size_t count=0;
        for (const auto& item:list) { auto p=Decode(item); count+=p.points.size(); if (!ids.insert(p.id).second || count>MaxTotalPoints) throw std::runtime_error(Tr("Invalid route data.")); loaded.push_back(std::move(p)); }
        enabled=data.value("enabled",false); paths=std::move(loaded);
    }
    void RaidMode::Reset()
    { *this=RaidMode{}; }

    nlohmann::json RaidMode::ExportFolder(std::string_view character,std::string_view folder) const
    {
        auto result=json::array();
        for (const auto& p:paths) if (p.character==character && p.folderId==folder && p.points.size()>1) result.push_back(Encode(p,true));
        return result;
    }
    std::vector<RaidPath> RaidMode::DecodeSharedPaths(const nlohmann::json& data,const std::string& character)
    {
        if (!data.is_array() || data.size()>MaxPaths) throw std::runtime_error(Tr("Invalid route data."));
        std::vector<RaidPath> result;
        size_t count=0;
        for (const auto& item:data) { auto p=Decode(item,&character); count+=p.points.size();
            if (count>MaxTotalPoints) throw std::runtime_error(Tr("Invalid route data.")); result.push_back(std::move(p)); }
        return result;
    }

    bool RaidMode::DrawAppearance()
    {
        bool changed=false;
        changed|=ImGui::Checkbox(Tr("Routes on map"),&appearance.map);
        changed|=ImGui::Checkbox(Tr("Routes on minimap"),&appearance.minimap);
        changed|=ImGui::Checkbox(Tr("Routes in 3D"),&appearance.world);
        changed|=ImGui::Checkbox(Tr("Direction arrows"),&appearance.arrows);
        changed|=ImGui::Checkbox(Tr("Animate routes"),&appearance.animate);
        const auto slider=[&](const char* label,float* value,float low,float high,const char* format) {
            ImGui::TextUnformatted(Tr(label)); ImGui::SetNextItemWidth(-1); ImGui::PushID(label);
            const bool edit=ImGui::SliderFloat("##value",value,low,high,format,ImGuiSliderFlags_AlwaysClamp); ImGui::PopID(); return edit; };
        changed|=slider("Route opacity",&appearance.opacity,.1f,1.f,"%.2f");
        changed|=slider("Ground beam width",&appearance.width,.15f,2.f,"%.2f m");
        changed|=slider("Map line width",&appearance.mapWidth,1.f,8.f,"%.1f px");
        changed|=slider("Route glow",&appearance.glow,0.f,1.f,"%.2f");
        changed|=slider("Flow speed",&appearance.speed,.2f,3.f,"%.1fx");
        return changed;
    }
}
