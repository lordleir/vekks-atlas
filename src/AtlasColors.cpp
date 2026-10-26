#include "AtlasColors.h"

namespace cmm
{
    uint32_t MarkerColor(Icon icon)
    {
        switch (icon)
        {
        case Icon::House: case Icon::Mission: case Icon::Star: case Icon::Chest: return Rgba(231, 184, 75);
        case Icon::Swords: return Rgba(206, 72, 58);
        case Icon::Pickaxe: case Icon::Flag: return Rgba(74, 148, 204);
        case Icon::Check: return Rgba(85, 183, 101);
        case Icon::Portal: return Rgba(191, 80, 232);
        case Icon::ArrowCart: return Rgba(108, 190, 59);
        case Icon::Catapult: return Rgba(225, 154, 45);
        case Icon::Trebuchet: return Rgba(45, 125, 235);
        case Icon::FlameRam: return Rgba(218, 72, 49);
        case Icon::ShieldGenerator: return Rgba(72, 205, 205);
        case Icon::RaidSupport: return Rgba(242, 194, 52);
        case Icon::RaidDamage: return Rgba(231, 74, 49);
        case Icon::RaidTank: return Rgba(52, 148, 241);
        case Icon::RaidHeal: return Rgba(69, 228, 91);
        }
        return Rgba(255, 255, 255);
    }
}
