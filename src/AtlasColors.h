#pragma once

#include "AtlasModel.h"

#include <cstdint>

namespace cmm
{
    constexpr uint32_t Rgba(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255)
    {
        return static_cast<uint32_t>(red) | (static_cast<uint32_t>(green) << 8) |
            (static_cast<uint32_t>(blue) << 16) | (static_cast<uint32_t>(alpha) << 24);
    }

    uint32_t MarkerColor(Icon icon);
}
