#pragma once
#include "AtlasModel.h"
#include <cstdint>
#include <string>
#include <vector>

namespace cmm
{
    struct RaidPath
    {
        std::string id, character, name;
        uint32_t mapId{};
        uint32_t color{0xff58cee8};
        bool visible{true};
        std::vector<WorldVec3> points;
    };
}
