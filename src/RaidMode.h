#pragma once
#include "RaidPaths.h"
#include "WorldBeacons.h"
#include <nlohmann/json.hpp>
#include <array>
#include <functional>

namespace cmm
{
    struct RaidContext
    {
        std::string character;
        uint32_t mapId{};
        WorldVec3 avatar;
        bool valid{}, mapOpen{}, writable{};
        float scale{1};
    };

    class RaidMode
    {
    public:
        bool enabled{}, windowVisible{}, eraser{};
        std::vector<RaidPath> paths;
        bool Button(float scale);
        void ToggleWindow();
        bool Update(const RaidContext& context);
        bool Finish();
        bool Recording() const { return !recordingId_.empty(); }
        bool Draw(const RaidContext& context, const std::function<void(Icon)>& place,
            const std::function<void(ImDrawList*,ImVec2,float,Icon)>& icon,
            const std::function<void(const std::string&)>& deleteMarker);
        void SelectForDeletion(std::string pathId, std::string markerId);
        nlohmann::json Save() const;
        void Load(const nlohmann::json& data);
        void Reset();
    private:
        std::string recordingId_, selectedId_, deletePath_, deleteMarker_, message_;
        std::array<char,128> name_{};
        std::array<float,3> color_{.91f,.81f,.34f};
        double lastSample_{};
        WorldVec3 lastAvatar_{};
        bool haveAvatar_{};
        std::string contextCharacter_;
        uint32_t contextMap_{};
        RaidPath* Find(const std::string& id);
        bool Start(const RaidContext& context);
        void Select(const RaidPath& path);
    };
}
