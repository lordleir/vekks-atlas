#pragma once
#include "AtlasModel.h"
#include <cstdint>
#include <string>
#include <vector>

namespace cmm
{
    enum class PathStyle : uint8_t { Solid, Dashed, Dotted, Arrows };
    struct RaidAppearance
    {
        bool map{true}, minimap{true}, world{true}, arrows{true}, animate{true};
        float opacity{.85f}, width{.55f}, mapWidth{3.f}, glow{.7f}, speed{1.f};
    };
    struct RaidMapAnchor { WorldVec3 world; Point continent; };
    struct RaidPath
    {
        std::string id, character, name;
        uint32_t mapId{};
        uint32_t color{0xff58cee8};
        bool visible{true};
        std::vector<WorldVec3> points;
        std::optional<RaidMapAnchor> mapAnchor;
        std::string folderId;
        bool folderVisible{true}; // derived each frame, not persisted
        PathStyle style{PathStyle::Solid};
    };
}
