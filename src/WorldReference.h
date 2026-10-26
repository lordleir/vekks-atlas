#pragma once

#include "WorldBeacons.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace cmm
{
    class WorldReferenceTracker
    {
    public:
        const WorldMapReference& GetOrCreate(uint32_t mapId, std::string_view character,
            const WorldBeaconCamera& camera);
        void Reset() noexcept;

    private:
        uint32_t mapId_{};
        std::string character_;
        std::optional<WorldMapReference> reference_;
    };
}
