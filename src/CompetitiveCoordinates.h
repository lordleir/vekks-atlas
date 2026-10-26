#pragma once

#include "AtlasModel.h"

#include <cstdint>
#include <optional>

namespace cmm
{
    std::optional<Point> CompetitivePlayerContinent(uint32_t mapId, const WorldVec3& avatarPosition);

}
