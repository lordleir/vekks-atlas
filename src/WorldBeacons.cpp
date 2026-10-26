#include "WorldBeacons.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace cmm
{
    namespace
    {
        constexpr float MetresPerContinentUnit = 0.6096f;
        constexpr float NearDepth = 0.20f;
        constexpr float Pi = 3.141592654f;
        constexpr float PillarHeight = 150.0f;
        constexpr size_t MaxDetailedBeacons = 32;

        bool Finite(float v) { return std::isfinite(v); }
        bool Finite(WorldVec3 v) { return Finite(v.x) && Finite(v.y) && Finite(v.z); }
        WorldVec3 Subtract(WorldVec3 a, WorldVec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
        float Dot(WorldVec3 a, WorldVec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
        WorldVec3 Cross(WorldVec3 a, WorldVec3 b)
        { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
        bool Normalize(WorldVec3& v)
        {
            const float squared = Dot(v,v);
            if (!Finite(squared) || squared < 0.000001f) return false;
            const float inv = 1.0f/std::sqrt(squared);
            v = {v.x*inv,v.y*inv,v.z*inv};
            return Finite(v);
        }
        ImU32 WithAlpha(ImU32 color, float alpha)
        {
            return (color & ~IM_COL32_A_MASK) |
                (static_cast<ImU32>(std::clamp(static_cast<int>(alpha),0,255)) << IM_COL32_A_SHIFT);
        }

        ImU32 SaturatedColor(ImU32 color)
        {
            // Increase beam saturation without changing its hue.
            const int r = (color >> IM_COL32_R_SHIFT) & 255;
            const int g = (color >> IM_COL32_G_SHIFT) & 255;
            const int b = (color >> IM_COL32_B_SHIFT) & 255;
            const int peak = std::max({r,g,b});
            if (peak == 0) return color;
            const auto channel = [peak](int value) {
                return std::clamp(static_cast<int>(245.0f - (peak-value)*1.25f*245.0f/peak), 0, 255);
            };
            return IM_COL32(channel(r),channel(g),channel(b),255);
        }

        struct Projection
        {
            WorldVec3 origin, forward, right, up;
            float tanY{},tanX{};
            ImVec2 display;
            WorldVec3 View(WorldVec3 world) const
            {
                const auto relative = Subtract(world,origin);
                return {Dot(relative,right),Dot(relative,up),Dot(relative,forward)};
            }
            ImVec2 Screen(WorldVec3 view) const
            {
                return {(1.0f+view.x/(view.z*tanX))*.5f*display.x,
                    (1.0f-view.y/(view.z*tanY))*.5f*display.y};
            }
            // Clip before perspective division.
            float Plane(WorldVec3 v, int plane) const
            {
                switch (plane)
                {
                case 0: return v.z-NearDepth;
                case 1: return v.x+v.z*tanX;
                case 2: return v.z*tanX-v.x;
                case 3: return v.y+v.z*tanY;
                default: return v.z*tanY-v.y;
                }
            }
            bool Visible(WorldVec3 view) const
            {
                if (!Finite(view)) return false;
                for (int i=0;i<5;++i) if (Plane(view,i)<0.0f) return false;
                return true;
            }
        };

        bool BuildProjection(const WorldBeaconCamera& camera, Projection& p)
        {
            if (!Finite(camera.avatarPosition) || !Finite(camera.cameraPosition) ||
                !Finite(camera.cameraFront) || !Finite(camera.cameraTop) ||
                !Finite(camera.displaySize.x) || !Finite(camera.displaySize.y) ||
                camera.displaySize.x<1.0f || camera.displaySize.y<1.0f) return false;
            p.origin=camera.cameraPosition;
            p.forward=camera.cameraFront;
            if (!Normalize(p.forward)) return false;
            auto top=camera.cameraTop;
            if (!Normalize(top)) top={0,1,0}; // GW2 may publish a zero top vector.
            p.right=Cross(top,p.forward); // Mumble's left-handed, Y-up space.
            if (!Normalize(p.right))
            {
                p.right=Cross({0,0,1},p.forward);
                if (!Normalize(p.right)) return false;
            }
            p.up=Cross(p.forward,p.right);
            if (!Normalize(p.up)) return false;
            const float fov=Finite(camera.verticalFov) && camera.verticalFov>=.20f && camera.verticalFov<=2.80f
                ? camera.verticalFov : 1.22173048f;
            p.tanY=std::tan(fov*.5f);
            p.tanX=p.tanY*camera.displaySize.x/camera.displaySize.y;
            p.display=camera.displaySize;
            return Finite(p.tanX) && p.tanX>.0001f && Finite(p.tanY) && p.tanY>.0001f;
        }

        struct Vertex { WorldVec3 view; float alpha; };
        Vertex Lerp(Vertex a, Vertex b, float t)
        {
            return {{a.view.x+(b.view.x-a.view.x)*t,a.view.y+(b.view.y-a.view.y)*t,
                a.view.z+(b.view.z-a.view.z)*t},a.alpha+(b.alpha-a.alpha)*t};
        }
        bool VertexRoom(const ImDrawList* draw, unsigned count)
        {
            return sizeof(ImDrawIdx)>2 || (draw->Flags & ImDrawListFlags_AllowVtxOffset) ||
                draw->_VtxCurrentIdx+count<65000;
        }

        // Five clipping planes can produce at most nine vertices.
        void Quad(ImDrawList* draw, const Projection& p, const std::array<WorldVec3,4>& points,
            const std::array<float,4>& alpha, ImU32 color)
        {
            std::array<Vertex,16> vertices{}, output{};
            int count=4;
            for (int i=0;i<4;++i)
            {
                vertices[i]={p.View(points[i]),alpha[i]};
                if (!Finite(vertices[i].view)) return;
            }
            for (int plane=0;plane<5;++plane)
            {
                int out=0;
                for (int i=0;i<count;++i)
                {
                    const auto a=vertices[i],b=vertices[(i+1)%count];
                    const float da=p.Plane(a.view,plane), db=p.Plane(b.view,plane);
                    if (da>=0) output[out++]=a;
                    if ((da>=0)!=(db>=0)) output[out++]=Lerp(a,b,da/(da-db));
                }
                count=out;
                if (count<3) return;
                vertices=output;
            }
            const unsigned vertexCount=static_cast<unsigned>((count-2)*3);
            if (!VertexRoom(draw,vertexCount)) return;
            const auto uv=ImGui::GetFontTexUvWhitePixel();
            draw->PrimReserve(static_cast<int>(vertexCount),static_cast<int>(vertexCount));
            for (int i=1;i<count-1;++i)
            {
                const unsigned index=draw->_VtxCurrentIdx;
                draw->PrimWriteIdx(static_cast<ImDrawIdx>(index));
                draw->PrimWriteIdx(static_cast<ImDrawIdx>(index+1));
                draw->PrimWriteIdx(static_cast<ImDrawIdx>(index+2));
                for (int v : {0,i,i+1})
                    draw->PrimWriteVtx(p.Screen(vertices[v].view),uv,WithAlpha(color,vertices[v].alpha));
            }
        }

        void WorldLine(ImDrawList* draw,const Projection& p,WorldVec3 a,WorldVec3 b,ImU32 color,float width)
        {
            Vertex av{p.View(a),0},bv{p.View(b),0};
            if (!Finite(av.view) || !Finite(bv.view)) return;
            for (int plane=0;plane<5;++plane)
            {
                const float da=p.Plane(av.view,plane),db=p.Plane(bv.view,plane);
                if (da<0 && db<0) return;
                if ((da<0)!=(db<0))
                {
                    const auto clipped=Lerp(av,bv,da/(da-db));
                    if (da<0) av=clipped; else bv=clipped;
                }
            }
            if (VertexRoom(draw,16)) draw->AddLine(p.Screen(av.view),p.Screen(bv.view),color,width);
        }

        WorldVec3 Offset(WorldVec3 base,WorldVec3 side,float x,float y)
        { return {base.x+side.x*x,base.y+y,base.z+side.z*x}; }

        // Camera-facing width, but the long axis is always world +Y.
        void Ribbon(ImDrawList* draw,const Projection& p,WorldVec3 base,WorldVec3 side,
            float width,float height,ImU32 color,float opacity,float time,float phase)
        {
            constexpr float xs[]{-1,-.65f,-.32f,0,.32f,.65f,1};
            constexpr float ys[]{0,.035f,.14f,.38f,.70f,1};
            const auto intensity=[&](float x,float y)
            {
                const float radial=std::max(0.0f,(std::exp(-x*x*4.5f)-.011109f)/.988891f);
                const float ends=std::min(1.0f,y/.035f)*std::pow(1.0f-y,1.15f);
                // Keep the mist centered as the billboard rotates.
                const float flow=.94f+.04f*std::sin(y*17.0f-time*1.8f+phase)+
                    .02f*std::sin(y*31.0f-time*1.1f+phase);
                return opacity*radial*ends*flow;
            };
            for (int y=0;y<5;++y) for (int x=0;x<6;++x)
            {
                const float x0=xs[x],x1=xs[x+1],y0=ys[y],y1=ys[y+1];
                Quad(draw,p,{Offset(base,side,x0*width,y0*height),Offset(base,side,x1*width,y0*height),
                    Offset(base,side,x1*width,y1*height),Offset(base,side,x0*width,y1*height)},
                    {intensity(x0,y0),intensity(x1,y0),intensity(x1,y1),intensity(x0,y1)},color);
            }
        }

        struct PreparedBeacon
        {
            const WorldBeacon* marker{};
            WorldVec3 base;
            float distance{},beamAlpha{},annotationAlpha{},phase{};
        };

        void Annotation(ImDrawList* draw,const Projection& p,const PreparedBeacon& b,const WorldBeaconStyle& style,std::string* hoveredMarker)
        {
            const auto anchor=p.View({b.base.x,b.base.y+.5f,b.base.z});
            if (!p.Visible(anchor) || !VertexRoom(draw,1500)) return;
            const auto ground=p.Screen(anchor);
            const float scale=std::clamp(style.uiScale,.5f,4.0f);
            const float iconScale=scale*std::clamp(style.iconScale,.5f,2.0f);
            // Camera depth drives perspective. The readable floor is approached
            // smoothly, so distant markers do not all snap to one fixed size.
            const float referenceDepth=40.0f*std::tan(1.22173048f*.5f)/p.tanY;
            const float perspective=referenceDepth/(referenceDepth+anchor.z);
            const float textScale=scale*std::clamp(style.textScale,.7f,1.6f)*(.72f+.28f*perspective);
            const float side=(14.0f+42.0f*perspective)*iconScale;
            const float lift=(3.0f+4.0f*perspective)*iconScale;
            const auto pick=[&](ImVec2 lo,ImVec2 hi)
            {
                const auto mouse=ImGui::GetIO().MousePos;
                if (hoveredMarker && !b.marker->id.empty() && mouse.x>=lo.x && mouse.x<=hi.x && mouse.y>=lo.y && mouse.y<=hi.y)
                { *hoveredMarker=b.marker->id; draw->AddRect(lo,hi,IM_COL32(255,120,80,255),2*scale,0,2*scale); }
            };
            float labelBottom=ground.y-lift;
            if (style.showIcons && b.marker->icon)
            {
                const ImVec2 lo{ground.x-side*.5f,ground.y-lift-side},hi{ground.x+side*.5f,ground.y-lift};
                draw->AddLine(ground,{ground.x,hi.y},WithAlpha(b.marker->color,
                    175*b.annotationAlpha*std::clamp(style.iconOpacity,.1f,1.0f)),1.5f*iconScale);
                draw->AddImage(b.marker->icon,{lo.x+2*iconScale,lo.y+2*iconScale},{hi.x+2*iconScale,hi.y+2*iconScale},
                    {0,0},{1,1},WithAlpha(IM_COL32_BLACK,190*b.annotationAlpha*style.iconOpacity));
                draw->AddImage(b.marker->icon,lo,hi,{0,0},{1,1},WithAlpha(IM_COL32_WHITE,255*b.annotationAlpha*style.iconOpacity));
                pick(lo,hi);
                labelBottom=lo.y-4*textScale;
            }
            const bool showName=style.showNames && !b.marker->name.empty();
            if (!showName && !style.showDistance) return;
            const float fontSize=ImGui::GetFontSize()*textScale;
            auto* font=ImGui::GetFont();
            // Single-line bounded label with intact UTF-8 boundaries.
            size_t end=showName ? std::min(size_t{160},b.marker->name.size()) : 0;
            while (end<b.marker->name.size() && end>0 &&
                (static_cast<unsigned char>(b.marker->name[end])&0xC0)==0x80) --end;
            std::string label=showName ? std::string(b.marker->name.substr(0,end)) : std::string{};
            bool shortened=showName && end<b.marker->name.size();
            for (auto& c:label) if (c=='\n' || c=='\r' || c=='\t') c=' ';
            auto size=font->CalcTextSizeA(fontSize,10000,0,label.c_str());
            while (size.x>230*scale && !label.empty())
            {
                size_t cut=label.size()-1;
                while (cut>0 && (static_cast<unsigned char>(label[cut])&0xC0)==0x80) --cut;
                label.resize(cut);
                shortened=true;
                size=font->CalcTextSizeA(fontSize,10000,0,label.c_str());
            }
            if (shortened) label+="...";
            char distanceText[32]{};
            if (style.showDistance)
            {
                const float units=b.distance/MetresPerContinentUnit;
                if (units>=10000.0f) snprintf(distanceText,sizeof(distanceText),"%.1fk u",units/1000.0f);
                else snprintf(distanceText,sizeof(distanceText),"%.0f u",units);
            }
            const std::string separator=showName && style.showDistance ? "  ·  " : "";
            const std::string fullLabel=label+separator+distanceText;
            size=font->CalcTextSizeA(fontSize,10000,0,fullLabel.c_str());
            const ImVec2 text{ground.x-size.x*.5f,labelBottom-size.y};
            pick({text.x-5*textScale,text.y-3*textScale},{text.x+size.x+5*textScale,labelBottom+3*textScale});
            const float backgroundOpacity=std::max(showName ? style.nameOpacity : 0.0f,
                style.showDistance ? style.distanceOpacity : 0.0f);
            draw->AddRectFilled({text.x-5*textScale,text.y-3*textScale},{text.x+size.x+5*textScale,labelBottom+3*textScale},
                WithAlpha(IM_COL32_BLACK,175*b.annotationAlpha*backgroundOpacity),4*textScale);
            float x=text.x;
            const auto part=[&](const std::string& value,float opacity,ImU32 color)
            {
                if (value.empty()) return;
                draw->AddText(font,fontSize,{x+textScale,text.y+textScale},WithAlpha(IM_COL32_BLACK,240*b.annotationAlpha*opacity),value.c_str());
                draw->AddText(font,fontSize,{x,text.y},WithAlpha(color,255*b.annotationAlpha*opacity),value.c_str());
                x+=font->CalcTextSizeA(fontSize,10000,0,value.c_str()).x;
            };
            part(label,std::clamp(style.nameOpacity,.1f,1.0f),IM_COL32_WHITE);
            part(separator,std::max(style.nameOpacity,style.distanceOpacity),IM_COL32(190,198,198,255));
            part(distanceText,std::clamp(style.distanceOpacity,.1f,1.0f),IM_COL32(203,226,229,255));
        }
    }

    bool IsWorldBeaconCameraValid(const WorldBeaconCamera& camera)
    {
        Projection projection;
        return BuildProjection(camera, projection);
    }

    float WorldUnitsToMetres(float units) { return units * MetresPerContinentUnit; }
    float MetresToWorldUnits(float metres) { return metres / MetresPerContinentUnit; }

    WorldBeaconAlphas ComputeWorldBeaconAlphas(float distanceMetres,float maximumMetres,float beamOpacity)
    {
        if (!Finite(distanceMetres) || !Finite(maximumMetres) || !Finite(beamOpacity) || maximumMetres<=0) return {};
        const float fade=std::clamp((maximumMetres-distanceMetres)/(maximumMetres*.25f),0.0f,1.0f);
        return {fade*std::clamp(beamOpacity,0.0f,1.0f),fade};
    }

    std::size_t DrawWorldBeacons(ImDrawList* draw,const WorldBeaconCamera& camera,const WorldMapReference& reference,
        const std::vector<WorldBeacon>& beacons,const WorldBeaconStyle& style,std::string* hoveredMarker)
    {
        if (!draw || beacons.empty() || !Finite(style.opacity) || style.opacity<=0 ||
            !Finite(style.maxDistance) || style.maxDistance<25 || !Finite(style.uiScale) || style.uiScale<=0) return 0;
        Projection p;
        if (!BuildProjection(camera,p)) return 0;
        const float maximum=std::clamp(style.maxDistance,25.0f,10000.0f);
        const float opacity=std::clamp(style.opacity,0.0f,1.0f);
        const float scale=std::clamp(style.uiScale,.5f,4.0f);
        const float beamWidth=Finite(style.beamWidth) ? std::clamp(style.beamWidth,.25f,3.0f) : 1.0f;
        const float time=static_cast<float>(std::fmod(ImGui::GetTime(),3600.0));
        std::vector<PreparedBeacon> prepared;
        prepared.reserve(std::min(beacons.size(),MaxDetailedBeacons));
        for (const auto& marker:beacons)
        {
            if (!Finite(marker.worldHeight)) continue;
            WorldVec3 base;
            if (marker.worldPosition)
            {
                base=*marker.worldPosition;
                base.y=marker.worldHeight; // manual elevation override
            }
            else
            {
                if (!Finite(marker.continentX) || !Finite(marker.continentY) ||
                    !Finite(reference.worldPosition) || !Finite(reference.continentX) || !Finite(reference.continentY)) continue;
                const float dx=(marker.continentX-reference.continentX)*MetresPerContinentUnit;
                const float dz=-(marker.continentY-reference.continentY)*MetresPerContinentUnit;
                base={reference.worldPosition.x+dx,marker.worldHeight,reference.worldPosition.z+dz};
            }
            if (!Finite(base)) continue;
            const auto relative=Subtract(base,camera.avatarPosition);
            const float distance=std::sqrt(Dot(relative,relative));
            if (!Finite(distance) || distance>maximum) continue;
            const auto alphas=ComputeWorldBeaconAlphas(distance,maximum,opacity);
            if (alphas.annotation<.01f) continue;
            // Keep partially visible beams.
            const auto center=p.View({base.x,base.y+PillarHeight*.5f,base.z});
            const float radius=PillarHeight*.5f+9.0f*beamWidth+3.0f;
            bool culled=false;
            for (int plane=0;plane<5;++plane)
            {
                const float normal=plane==0 ? 1.0f : std::sqrt(1.0f+(plane<3?p.tanX*p.tanX:p.tanY*p.tanY));
                if (p.Plane(center,plane)<-radius*normal) { culled=true; break; }
            }
            if (culled) continue;
            const float phase=std::fmod(std::abs(marker.worldPosition ? base.x*.13f+base.z*.07f :
                marker.continentX*.13f+marker.continentY*.07f),2*Pi);
            PreparedBeacon candidate{&marker,base,distance,alphas.beam,alphas.annotation,phase};
            if (prepared.size()<MaxDetailedBeacons) prepared.push_back(candidate);
            else
            {
                auto farthest=std::max_element(prepared.begin(),prepared.end(),[](const auto& a,const auto& b){return a.distance<b.distance;});
                if (candidate.distance<farthest->distance) *farthest=candidate;
            }
        }
        std::sort(prepared.begin(),prepared.end(),[](const auto& a,const auto& b){return a.distance>b.distance;});
        draw->PushClipRect({0,0},camera.displaySize,true);
        draw->PushTextureID(ImGui::GetIO().Fonts->TexID);
        size_t drawn=0;
        for (const auto& b:prepared)
        {
            if (!VertexRoom(draw,3000)) break;
            const auto before=draw->VtxBuffer.Size;
            WorldVec3 side{camera.cameraPosition.z-b.base.z,0,b.base.x-camera.cameraPosition.x};
            if (!Normalize(side)) side={1,0,0};
            const float width=9.0f*beamWidth; // fixed world radius, independent of movement
            const float pulse=.97f+.03f*std::sin(time*1.4f+b.phase);
            const float closeFade=std::clamp(b.distance/6.0f,0.0f,1.0f);
            const float alpha=b.beamAlpha*pulse*closeFade;
            // Layered shaft with a colored core.
            const ImU32 beamColor = SaturatedColor(b.marker->color);
            Ribbon(draw,p,b.base,side,width,PillarHeight,beamColor,195*alpha,time,b.phase);
            Ribbon(draw,p,b.base,side,width*.58f,PillarHeight*.95f,beamColor,245*alpha,time,b.phase);
            for (int ring=0;ring<2;++ring)
            {
                const float r=ring==0 ? 2.4f : 3.2f;
                for (int i=0;i<32;++i)
                {
                    const float a=2*Pi*static_cast<float>(i)/32,beta=2*Pi*static_cast<float>(i+1)/32;
                    const float shimmer=.55f+.45f*std::sin(a*3-time*2+b.phase);
                    const WorldVec3 v0{b.base.x+r*std::cos(a),b.base.y+.08f,b.base.z+r*std::sin(a)};
                    const WorldVec3 v1{b.base.x+r*std::cos(beta),b.base.y+.08f,b.base.z+r*std::sin(beta)};
                    WorldLine(draw,p,v0,v1,WithAlpha(b.marker->color,(ring==0?160:95)*b.beamAlpha*shimmer),2*scale);
                }
            }
            // Rising particles around the beam.
            for (int i=0;i<10;++i)
            {
                const float t=std::fmod(time*.16f+static_cast<float>(i)*.103f+b.phase*.1f,1.0f);
                const float angle=b.phase+static_cast<float>(i)*2.4f+time*.55f;
                const float r=width*(.25f+.25f*std::sin(static_cast<float>(i)));
                const WorldVec3 start{b.base.x+r*std::cos(angle),b.base.y+2+t*PillarHeight*.75f,b.base.z+r*std::sin(angle)};
                const float a=110*alpha*std::sin(t*Pi),w=width*.065f;
                Quad(draw,p,{Offset(start,side,-w,0),Offset(start,side,0,-2),start,Offset(start,side,0,2)},
                    {0,0,a,0},b.marker->color);
                Quad(draw,p,{start,Offset(start,side,0,-2),Offset(start,side,w,0),Offset(start,side,0,2)},
                    {a,0,0,0},b.marker->color);
            }
            if (draw->VtxBuffer.Size>before) ++drawn;
        }
        for (const auto& b:prepared) Annotation(draw,p,b,style,hoveredMarker);
        draw->PopTextureID();
        draw->PopClipRect();
        return drawn;
    }

    std::string DrawRaidPaths(ImDrawList* draw,const WorldBeaconCamera& camera,const std::vector<RaidPath>& paths,
        std::string_view character,uint32_t mapId,float maxDistance,float scale,bool picking)
    {
        Projection p;
        if (!draw || !BuildProjection(camera,p) || !Finite(maxDistance) || maxDistance<=0) return {};
        std::string hovered; float closest=64*scale*scale;
        const auto mouse=ImGui::GetIO().MousePos;
        unsigned segments=0;
        const auto pointAt=[](WorldVec3 a,WorldVec3 b,float t) { return WorldVec3{a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t+.12f,a.z+(b.z-a.z)*t}; };
        draw->PushClipRect({0,0},camera.displaySize,true);
        for (const auto& path:paths)
        {
            if (!path.visible || path.mapId!=mapId || path.character!=character) continue;
            float arc=0;
            for (size_t i=1;i<path.points.size();++i)
            {
                const auto a=path.points[i-1],b=path.points[i];
                const auto delta=Subtract(b,a); const float length=std::sqrt(Dot(delta,delta));
                if (!Finite(length) || length<.001f || length>30) continue;
                const auto ra=Subtract(a,camera.avatarPosition),rb=Subtract(b,camera.avatarPosition);
                if (std::min(Dot(ra,ra),Dot(rb,rb))>maxDistance*maxDistance) { arc+=length; continue; }
                bool hit=false;
                if (picking)
                {
                    Vertex va{p.View(pointAt(a,b,0)),0},vb{p.View(pointAt(a,b,1)),0}; bool clipped=false;
                    for (int plane=0;plane<5;++plane)
                    {
                        float da=p.Plane(va.view,plane),db=p.Plane(vb.view,plane);
                        if (da<0 && db<0) { clipped=true; break; }
                        if ((da<0)!=(db<0)) { auto v=Lerp(va,vb,da/(da-db)); if (da<0) va=v; else vb=v; }
                    }
                    if (!clipped)
                    {
                        const auto sa=p.Screen(va.view),sb=p.Screen(vb.view);
                        const float dx=sb.x-sa.x,dy=sb.y-sa.y;
                        const float t=std::clamp(((mouse.x-sa.x)*dx+(mouse.y-sa.y)*dy)/std::max(.001f,dx*dx+dy*dy),0.f,1.f);
                        const float ex=mouse.x-sa.x-t*dx,ey=mouse.y-sa.y-t*dy,dist=ex*ex+ey*ey;
                        if (dist<closest) { closest=dist; hovered=path.id; hit=true; }
                    }
                }
                for (float walked=0;walked<length && segments<4000;)
                {
                    const float phase=std::fmod(arc+walked,1.5f);
                    const bool ink=phase<.9f;
                    const float end=std::min(length,walked+std::max(.001f,(ink?.9f:1.5f)-phase));
                    if (ink) { WorldLine(draw,p,pointAt(a,b,walked/length),pointAt(a,b,end/length),
                        hit?IM_COL32(255,120,80,255):path.color,(hit?4.f:2.4f)*scale); ++segments; }
                    walked=end;
                }
                for (float at=std::ceil((arc+.001f)/5.f)*5.f;at<=arc+length && segments<4000;at+=5.f)
                {
                    const auto tip=pointAt(a,b,(at-arc)/length);
                    WorldVec3 direction=delta; Normalize(direction);
                    WorldVec3 side=Cross({0,1,0},direction); if (!Normalize(side)) continue;
                    for (float sign:{-1.f,1.f})
                    { WorldVec3 tail{tip.x-direction.x*.8f+side.x*.45f*sign,tip.y-direction.y*.8f,tip.z-direction.z*.8f+side.z*.45f*sign};
                      WorldLine(draw,p,tip,tail,path.color,2.7f*scale); ++segments; }
                }
                arc+=length;
                if (segments>=4000 || !VertexRoom(draw,64)) break;
            }
            if (segments>=4000 || !VertexRoom(draw,64)) break;
        }
        draw->PopClipRect();
        return hovered;
    }
}
