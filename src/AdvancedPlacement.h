#pragma once

#include "AtlasModel.h"
#include "WorldBeacons.h"

#include <cstdint>
#include <optional>

namespace cmm
{
    enum class AdvancedPlacementError
    {
        None,
        StaleData,
        NotGameplay,
        MapMismatch,
        InvalidReference,
        InvalidCursor
    };

    struct AdvancedCursorSnapshot
    {
        uint32_t gameBuild{};
        uint32_t mapId{};
        bool gameplay{};
        WorldVec3 cursor;
        WorldVec3 characterPosition;
        WorldVec3 cameraPosition;
        WorldVec3 cameraFacing;
    };

    struct AdvancedPlacement
    {
        Point continent;
        float worldHeight{};
    };

    std::optional<AdvancedPlacement> ConvertAdvancedCursor(const AdvancedCursorSnapshot& snapshot,
        uint32_t currentMapId, const WorldMapReference& reference, const WorldVec3& mumbleCameraPosition,
        const WorldVec3& mumbleCameraFacing, AdvancedPlacementError& error);
}
