#pragma once
#include "AtlasModel.h"
#include <vector>
#include <string_view>

namespace cmm
{
    // Local IDs and character identity never leave the local store.
    struct ShareableMarker
    {
        uint32_t mapId{};
        uint32_t continentId{};
        int32_t floorId{};
        Point continent;
        Icon icon{Icon::House};
        std::string name;
        std::string folderName;
        std::optional<float> worldHeight;
        std::optional<WorldVec3> worldPosition;
    };
    struct ShareableFolder
    {
        std::string name;
        std::vector<ShareableMarker> markers;
    };
    bool ValidLabel(std::string_view text, size_t maxBytes, bool allowEmpty = true);
    bool EncodeMarkerLink(const ShareableMarker& marker, std::string& link, std::string& error);
    bool DecodeMarkerLink(std::string_view link, ShareableMarker& marker, std::string& error);
    bool EncodeFolderLink(const ShareableFolder& folder, std::string& link, std::string& error);
    bool DecodeFolderLink(std::string_view link, ShareableFolder& folder, std::string& error);
}
