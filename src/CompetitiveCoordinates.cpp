#include "CompetitiveCoordinates.h"

#include <array>
#include <cmath>

namespace cmm
{
    namespace
    {
        constexpr float InchesPerMetre = 39.37007874f;

        struct MapTransform
        {
            uint32_t id;
            float mapLeft, mapBottom, mapRight, mapTop;
            float continentLeft, continentTop, continentRight, continentBottom;
        };

        // ArenaNet /v2/maps rectangles for the competitive maps. The endpoint
        // exposes these specifically for translating world and map positions.
        constexpr std::array Transforms{
            MapTransform{38,   -36864,-36864,36864,36864, 8958,12798,12030,15870}, // Eternal Battlegrounds
            MapTransform{95,   -30720,-43008,30720,43008, 5630,11518,8190,15102},  // Green Alpine Borderlands
            MapTransform{96,   -30720,-43008,30720,43008, 12798,10878,15358,14462},// Blue Alpine Borderlands
            MapTransform{899,  -36864,-36864,36864,36864, 8958,12798,12030,15870}, // Obsidian Sanctum
            MapTransform{968,  -36864,-36864,36864,36864, 5994,8446,9066,11518},   // Edge of the Mists
            MapTransform{1099, -36864,-36864,36864,36864, 9214,8958,12286,12030},  // Desert Borderlands
            MapTransform{1315, -21504,-21504,21504,21504, 13798,8646,15590,10438}  // Armistice Bastion
        };

        const MapTransform* FindTransform(uint32_t mapId)
        {
            for (const auto& transform : Transforms) if (transform.id == mapId) return &transform;
            return nullptr;
        }
    }

    std::optional<Point> CompetitivePlayerContinent(uint32_t mapId, const WorldVec3& avatarPosition)
    {
        const auto* transform = FindTransform(mapId);
        if (!transform || !std::isfinite(avatarPosition.x) || !std::isfinite(avatarPosition.z)) return std::nullopt;
        const float mapWidth = transform->mapRight - transform->mapLeft;
        const float mapHeight = transform->mapTop - transform->mapBottom;
        if (mapWidth <= 0 || mapHeight <= 0) return std::nullopt;
        const float eventX = avatarPosition.x * InchesPerMetre;
        const float eventY = avatarPosition.z * InchesPerMetre;
        const float xFraction = (eventX - transform->mapLeft) / mapWidth;
        const float yFraction = (eventY - transform->mapBottom) / mapHeight;
        const Point result{
            transform->continentLeft + (transform->continentRight-transform->continentLeft)*xFraction,
            transform->continentTop + (transform->continentBottom-transform->continentTop)*(1.0f-yFraction)};
        if (!std::isfinite(result.x) || !std::isfinite(result.y)) return std::nullopt;
        return result;
    }

}
