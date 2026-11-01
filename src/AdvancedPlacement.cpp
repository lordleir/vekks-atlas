#include "AdvancedPlacement.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace cmm
{
    namespace
    {
        constexpr float MetresPerContinentUnit = 0.6096f;

        bool Finite(float value) { return std::isfinite(value); }
        bool Valid(const WorldVec3& value, float limit = 100000.0f)
        {
            return Finite(value.x) && Finite(value.y) && Finite(value.z) &&
                std::abs(value.x) <= limit && std::abs(value.y) <= limit && std::abs(value.z) <= limit;
        }

        float Component(const WorldVec3& value, int index)
        {
            return index == 0 ? value.x : index == 1 ? value.y : value.z;
        }

        bool Normalize(WorldVec3& value)
        {
            const float lengthSquared = value.x*value.x + value.y*value.y + value.z*value.z;
            if (!Finite(lengthSquared) || lengthSquared < 0.000001f) return false;
            const float inverseLength = 1.0f / std::sqrt(lengthSquared);
            value = {value.x*inverseLength, value.y*inverseLength, value.z*inverseLength};
            return Finite(value.x) && Finite(value.y) && Finite(value.z);
        }
    }

    std::optional<AdvancedPlacement> ConvertAdvancedCursor(const AdvancedCursorSnapshot& snapshot,
        uint32_t currentMapId, const WorldMapReference& reference, const WorldVec3& mumbleCameraPosition,
        const WorldVec3& mumbleCameraFacing, AdvancedPlacementError& error)
    {
        error = AdvancedPlacementError::None;
        if (snapshot.gameBuild == 0) { error = AdvancedPlacementError::StaleData; return std::nullopt; }
        if (!snapshot.gameplay) { error = AdvancedPlacementError::NotGameplay; return std::nullopt; }
        if (snapshot.mapId == 0 || snapshot.mapId != currentMapId) { error = AdvancedPlacementError::MapMismatch; return std::nullopt; }
        if (!Valid(reference.worldPosition) || !Valid(mumbleCameraPosition) || !Valid(mumbleCameraFacing) ||
            !Finite(reference.continentX) || !Finite(reference.continentY))
        { error = AdvancedPlacementError::InvalidReference; return std::nullopt; }
        if (!Valid(snapshot.cursor, 10000000.0f) || !Valid(snapshot.characterPosition, 10000000.0f) ||
            !Valid(snapshot.cameraPosition, 10000000.0f) || !Valid(snapshot.cameraFacing, 10000000.0f) ||
            (std::abs(snapshot.cursor.x) < .001f && std::abs(snapshot.cursor.y) < .001f && std::abs(snapshot.cursor.z) < .001f))
        { error = AdvancedPlacementError::InvalidCursor; return std::nullopt; }

        // Infer axes from relative camera vectors so MumbleLink position lag
        // while the character is moving cannot translate the cursor.
        constexpr int permutations[6][3]{{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
        constexpr float scales[]{1.0f, 0.0254f, 39.37007874f};
        const WorldVec3 mumbleCameraOffset{mumbleCameraPosition.x-reference.worldPosition.x,
            mumbleCameraPosition.y-reference.worldPosition.y, mumbleCameraPosition.z-reference.worldPosition.z};
        WorldVec3 normalizedMumbleFacing = mumbleCameraFacing;
        if (!Normalize(normalizedMumbleFacing))
        { error = AdvancedPlacementError::InvalidReference; return std::nullopt; }

        float bestError = std::numeric_limits<float>::max();
        float bestPositionError = std::numeric_limits<float>::max();
        float bestFacingDot = -1.0f;
        int bestPermutation = 0;
        int bestSigns = 0;
        float bestScale = 1.0f;
        for (int permutation = 0; permutation < 6; ++permutation)
            for (int signs = 0; signs < 8; ++signs)
                for (float scale : scales)
                {
                    float positionError = 0.0f;
                    WorldVec3 mappedFacing{};
                    for (int axis = 0; axis < 3; ++axis)
                    {
                        const float sign = (signs & (1 << axis)) ? -1.0f : 1.0f;
                        const int sourceAxis = permutations[permutation][axis];
                        const float cameraOffset = sign * scale *
                            (Component(snapshot.cameraPosition, sourceAxis) - Component(snapshot.characterPosition, sourceAxis));
                        const float positionDifference = cameraOffset - Component(mumbleCameraOffset, axis);
                        positionError += positionDifference * positionDifference;
                        const float facing = sign * Component(snapshot.cameraFacing, sourceAxis);
                        if (axis == 0) mappedFacing.x = facing;
                        else if (axis == 1) mappedFacing.y = facing;
                        else mappedFacing.z = facing;
                    }
                    if (!Normalize(mappedFacing)) continue;
                    const float facingDot = mappedFacing.x*normalizedMumbleFacing.x +
                        mappedFacing.y*normalizedMumbleFacing.y + mappedFacing.z*normalizedMumbleFacing.z;
                    const float candidateError = positionError + (1.0f-std::clamp(facingDot,-1.0f,1.0f))*100.0f;
                    if (candidateError < bestError)
                    {
                        bestError = candidateError;
                        bestPositionError = positionError;
                        bestFacingDot = facingDot;
                        bestPermutation = permutation;
                        bestSigns = signs;
                        bestScale = scale;
                    }
                }
        if (!Finite(bestError) || !Finite(bestPositionError) || std::sqrt(bestPositionError / 3.0f) > 25.0f ||
            !Finite(bestFacingDot) || bestFacingDot < 0.75f)
        { error = AdvancedPlacementError::InvalidReference; return std::nullopt; }

        WorldVec3 worldCursor{};
        for (int axis = 0; axis < 3; ++axis)
        {
            const float sign = (bestSigns & (1 << axis)) ? -1.0f : 1.0f;
            const int sourceAxis = permutations[bestPermutation][axis];
            const float delta = Component(snapshot.cursor, sourceAxis) - Component(snapshot.characterPosition, sourceAxis);
            const float mapped = Component(reference.worldPosition, axis) + sign * bestScale * delta;
            if (axis == 0) worldCursor.x = mapped;
            else if (axis == 1) worldCursor.y = mapped;
            else worldCursor.z = mapped;
        }
        if (!Valid(worldCursor)) { error = AdvancedPlacementError::InvalidCursor; return std::nullopt; }

        const float dx = worldCursor.x - reference.worldPosition.x;
        const float dz = worldCursor.z - reference.worldPosition.z;
        if (dx * dx + dz * dz > 25000.0f * 25000.0f)
        { error = AdvancedPlacementError::InvalidCursor; return std::nullopt; }

        AdvancedPlacement placement{{reference.continentX + dx / MetresPerContinentUnit,
            reference.continentY - dz / MetresPerContinentUnit}, worldCursor.y};
        if (!Finite(placement.continent.x) || !Finite(placement.continent.y) || !Finite(placement.worldHeight))
        { error = AdvancedPlacementError::InvalidCursor; return std::nullopt; }
        return placement;
    }
}
