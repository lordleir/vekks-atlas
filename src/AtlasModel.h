#pragma once
#include <cstdint>
#include <string>
#include <optional>

namespace cmm
{
    enum class Icon : uint8_t
    {
        House, Swords, Pickaxe, Mission, Star, Chest, Flag, Check,
        Portal, ArrowCart, Catapult, Trebuchet, FlameRam, ShieldGenerator,
        RaidSupport, RaidDamage, RaidTank, RaidHeal
    };
    struct Point { float x{}; float y{}; };
    struct WorldVec3 { float x{}; float y{}; float z{}; };
    struct Marker
    {
        std::string id;
        std::string character;
        uint32_t mapId{};
        uint32_t continentId{};
        int32_t floorId{};
        Point continent;
        Icon icon{Icon::House};
        std::string name;
        std::string folderId; // empty = Unfiled
        std::optional<float> worldHeight;
        std::optional<WorldVec3> worldPosition; // original MumbleLink metres; Y is vertical
    };
    struct Folder { std::string id; std::string character; std::string name; bool visible{true}; };
}
