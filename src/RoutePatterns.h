#pragma once
#include "RaidPaths.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace cmm
{
    inline constexpr float PathEndArrowScale=1.5f;

    struct PathArrowLayout
    {
        bool enabled{};
        double first{},spacing{1},gapHalf{};
        double endCut{std::numeric_limits<double>::infinity()};
    };

    inline PathArrowLayout MakePathArrowLayout(PathStyle style,float width,bool arrows,float arrowLength,float spacing)
    {
        PathArrowLayout result{arrows || style==PathStyle::Arrows,spacing,spacing,0};
        if(!result.enabled)return result;
        if(style==PathStyle::Dashed)
        {
            const double cell=std::max(2.,static_cast<double>(width)*4.);
            result.first=cell*4.3; result.spacing=cell*5;
            result.gapHalf=std::max(cell*.3,arrowLength*.95+width*.3);
        }
        else if(style==PathStyle::Dotted)
        {
            const double cell=std::max(.8,static_cast<double>(width)*2.5);
            result.first=result.spacing=cell*5;
            result.gapHalf=arrowLength*.95+width*.3;
        }
        return result;
    }

    template<class Arrow>
    void VisitPathArrows(const PathArrowLayout& arrows,double arc,double length,Arrow arrow)
    {
        if(!arrows.enabled || !std::isfinite(arc) || !std::isfinite(length) || length<=.00001 || arrows.spacing<=0)return;
        const double index=std::max(0.,std::ceil((arc+.00001-arrows.first)/arrows.spacing));
        for(double at=arrows.first+index*arrows.spacing;at<=std::min(arc+length,arrows.endCut-arrows.gapHalf);at+=arrows.spacing)
            arrow(static_cast<float>((at-arc)/length));
    }

    // All distances use the caller's units: metres in the world, pixels in 2D.
    // Shared arc positions keep dashes, dots and their arrow gaps aligned across samples.
    template<class Segment, class Dot>
    void VisitPathPattern(PathStyle style, double arc, double length, float width, Segment segment, Dot dot,
        const PathArrowLayout& arrows={})
    {
        if (!std::isfinite(arc) || !std::isfinite(length) || length<=.00001) return;
        if (style==PathStyle::Solid) { segment(0.f,1.f); return; }
        if (style==PathStyle::Arrows) return;
        const auto emit=[&](double lo,double hi) {
            hi=std::min(hi,arrows.endCut);
            if(hi<=lo)return;
            double cursor=lo;
            if(arrows.enabled && arrows.gapHalf>0)
            {
                const double index=std::max(0.,std::ceil((lo-arrows.gapHalf-arrows.first)/arrows.spacing));
                for(double at=arrows.first+index*arrows.spacing;at-arrows.gapHalf<hi;at+=arrows.spacing)
                {
                    const double end=std::min(hi,at-arrows.gapHalf);
                    if(end>cursor+.00001)segment(static_cast<float>((cursor-arc)/length),static_cast<float>((end-arc)/length));
                    cursor=std::max(cursor,at+arrows.gapHalf);
                }
            }
            if(hi>cursor+.00001)segment(static_cast<float>((cursor-arc)/length),static_cast<float>((hi-arc)/length));
        };
        if (style==PathStyle::Dotted)
        {
            const double spacing=std::max(.8,static_cast<double>(width)*2.5);
            for(double at=std::ceil((arc+.00001)/spacing)*spacing;at<=arc+length;at+=spacing) {
                if(at+width>arrows.endCut)continue;
                if(arrows.enabled && arrows.gapHalf>0) {
                    const double index=std::max(0.,std::round((at-arrows.first)/arrows.spacing));
                    const double center=arrows.first+index*arrows.spacing;
                    if(std::abs(at-center)<arrows.gapHalf+width)continue;
                }
                dot(static_cast<float>((at-arc)/length));
            }
            return;
        }
        const double period=std::max(2.,static_cast<double>(width)*4.);
        for(double start=std::floor(arc/period)*period;start<arc+length;start+=period)
        {
            const double lo=std::max(arc,start),hi=std::min(arc+length,start+period*.6);
            if(hi>lo)emit(lo,hi);
        }
    }
}
