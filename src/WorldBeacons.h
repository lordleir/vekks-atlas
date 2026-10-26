#pragma once

#include "AtlasModel.h"
#include "RaidPaths.h"

#include <imgui.h>

#include <cstddef>
#include <string_view>
#include <vector>

namespace cmm
{
    struct WorldBeaconCamera
    {
        WorldVec3 avatarPosition;
        WorldVec3 cameraPosition;
        WorldVec3 cameraFront;
        WorldVec3 cameraTop;
        float playerContinentX{};
        float playerContinentY{};
        float verticalFov{};
        ImVec2 displaySize{};
    };

    struct WorldBeacon
    {
        float continentX{};
        float continentY{};
        ImU32 color{};
        float worldHeight{}; // fixed base Y, metres; not sampled terrain
        ImTextureID icon{}; // borrowed Nexus texture, valid for this frame
        std::string_view name; // borrowed marker label; no per-frame full-name copies
        std::optional<WorldVec3> worldPosition;
        std::string_view id;
    };

    struct WorldMapReference
    {
        WorldVec3 worldPosition;
        float continentX{};
        float continentY{};
    };

    struct WorldBeaconStyle
    {
        float opacity{0.70f};
        float maxDistance{2500.0f};
        float uiScale{1.0f};
        bool showIcons{true};
        bool showNames{true};
        bool showDistance{false};
        float iconOpacity{1.0f};
        float nameOpacity{1.0f};
        float distanceOpacity{1.0f};
        float iconScale{1.0f};
        float textScale{1.0f};
        float beamWidth{1.0f};
    };

    struct WorldBeaconAlphas
    {
        float beam{};
        float annotation{};
    };

    float WorldUnitsToMetres(float units);
    float MetresToWorldUnits(float metres);
    WorldBeaconAlphas ComputeWorldBeaconAlphas(float distanceMetres, float maximumMetres, float beamOpacity);

    bool IsWorldBeaconCameraValid(const WorldBeaconCamera& camera);

    std::size_t DrawWorldBeacons(ImDrawList* draw, const WorldBeaconCamera& camera, const WorldMapReference& reference,
        const std::vector<WorldBeacon>& beacons, const WorldBeaconStyle& style, std::string* hoveredMarker = nullptr);
    std::string DrawRaidPaths(ImDrawList* draw, const WorldBeaconCamera& camera, const std::vector<RaidPath>& paths,
        std::string_view character, uint32_t mapId, float maxDistance, float scale, bool picking);
}
