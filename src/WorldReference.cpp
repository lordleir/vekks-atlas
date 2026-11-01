#include "WorldReference.h"

namespace cmm
{
    const WorldMapReference& WorldReferenceTracker::GetOrCreate(uint32_t mapId,
        std::string_view character, const WorldBeaconCamera& camera)
    {
        if (!reference_ || mapId_ != mapId || character_ != character)
        {
            mapId_ = mapId;
            character_ = character;
            reference_ = WorldMapReference{camera.avatarPosition,
                camera.playerContinentX, camera.playerContinentY};
        }
        return *reference_;
    }

    void WorldReferenceTracker::Reset() noexcept
    {
        mapId_ = 0;
        character_.clear();
        reference_.reset();
    }
}
