#pragma once
#include "RaidPaths.h"
#include "WorldBeacons.h"
#include <nlohmann/json.hpp>
#include <array>
#include <functional>
#include <string_view>

namespace cmm
{
    struct RaidContext
    {
        std::string character;
        uint32_t mapId{};
        WorldVec3 avatar;
        bool valid{}, mapOpen{}, writable{};
        float scale{1};
        std::optional<RaidMapAnchor> mapAnchor;
        std::function<void(std::string_view)> diagnostics;
    };

    class RaidMode
    {
    public:
        bool enabled{}, windowVisible{}, eraser{};
        std::vector<RaidPath> paths;
        RaidAppearance appearance;
        std::string activeFolder;
        nlohmann::json ExportFolder(std::string_view character,std::string_view folder) const;
        static std::vector<RaidPath> DecodeSharedPaths(const nlohmann::json& data,const std::string& character);
        bool DrawAppearance();
        bool Button(float scale);
        void ToggleWindow();
        bool Update(const RaidContext& context);
        bool Finish(std::string_view reason = "manual");
        bool SelectProject(const std::string& folderId);
        bool Recording() const { return !recordingId_.empty(); }
        bool Draw(const RaidContext& context, const std::function<void(Icon)>& place,
            const std::function<void(ImDrawList*,ImVec2,float,Icon)>& icon,
            const std::function<void(const std::string&)>& deleteMarker, const std::function<void(int)>& collections);
        void SelectForDeletion(std::string pathId, std::string markerId);
        nlohmann::json Save() const;
        void Load(const nlohmann::json& data);
        void Reset();
    private:
        std::string recordingId_, selectedId_, deletePath_, deleteMarker_, message_;
        std::array<char,128> name_{};
        std::array<char,128> search_{};
        int tab_{};
        bool sortAscending_{true};
        PathStyle newStyle_{PathStyle::Solid};
        std::array<float,3> color_{.91f,.81f,.34f};
        double lastSample_{};
        double invalidSince_{-1};
        WorldVec3 lastAvatar_{};
        bool haveAvatar_{};
        std::string contextCharacter_;
        uint32_t contextMap_{};
        std::function<void(std::string_view)> diagnosticLog_;
        RaidPath* Find(const std::string& id);
        bool Start(const RaidContext& context);
        void Select(const RaidPath& path);
    };
}
