#pragma once

#include "AtlasModel.h"
#include "WorldBeacons.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace cmm
{
    std::optional<Point> CompetitivePlayerContinent(uint32_t mapId, const WorldVec3& avatarPosition);

}
