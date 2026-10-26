#include "AtlasLocalization.h"
#include "AtlasColors.h"
#include "AtlasPreferences.h"
#include "AtlasShortcut.h"
#include <Windows.h>

#include <imgui.h>
#include <Mumble.h>
#include <Nexus.h>
#include <RTAPI.hpp>
#include <nlohmann/json.hpp>
#include "AdvancedPlacement.h"
#include "CompetitiveCoordinates.h"
#include "RaidMode.h"
#include "AtlasModel.h"
#include "MarkerLink.h"
#include "AtlasClipboard.h"
#include "AtlasTheme.h"
#include "WorldBeacons.h"
#include "WorldReference.h"
#include "SettingsLink.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cfloat>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>

namespace cmm
{
    using json = nlohmann::json;
    constexpr const char* AddonName = "Vekk's Atlas";
    constexpr const char* StorageFolder = "VekksAtlas";
    static_assert(IMGUI_VERSION_NUM == 18000, "Nexus API 6 requires ImGui 1.80, not a newer context layout.");
    static_assert(offsetof(Mumble::Data, Context) == 1108);
    static_assert(offsetof(Mumble::Context, Compass) == 52);
    static_assert(offsetof(Mumble::Context, ProcessID) == 80);
    static_assert(sizeof(RTAPI::RealTimeData) == 512);
    static_assert(offsetof(RTAPI::RealTimeData, Cursor) == 28);
    static_assert(offsetof(RTAPI::RealTimeData, CharacterPosition) == 432);
    static_assert(offsetof(RTAPI::RealTimeData, CameraPosition) == 472);

    struct View
    {
        float left{};
        float top{};
        float width{};
        float height{};
        float centerX{};
        float centerY{};
        float scale{}; // continent-coordinate units per screen pixel, published by MumbleLink.
        bool valid{};
        float rotation{};
    };

    struct State
    {
        const char* reason{Tr("No MumbleLink/Nexus data")};
        std::string character;
        uint32_t mapId{};
        uint32_t shardId{};
        uint32_t instanceId{};
        Mumble::EMapType mapType{Mumble::EMapType::AutoRedirect};
        bool mapOpen{};
        bool textFocused{};
        bool avatarValid{};
        float uiScale{1.0f};
        View fullMap;
        View minimap;
        WorldBeaconCamera worldCamera;
    };

    AddonAPI_t* Api = nullptr;
    Mumble::Data* MumbleLink = nullptr;
    NexusLinkData_t* NexusLink = nullptr;
    std::mutex StateMutex;
    std::atomic<bool> Ready{false};
    bool StorageWritable = true;
    std::string StorageError;
    std::filesystem::path DataFile;
    std::filesystem::path SchemaOneSource;
    std::vector<Marker> Markers;
    std::vector<Folder> Folders;
    Preferences AtlasPreferences;
    std::unordered_map<std::string, bool> UnfiledVisibility;
    std::unordered_map<uint32_t, std::string> MapNames;
    bool Dirty = false;
    std::chrono::steady_clock::time_point DirtySince{};
    bool RenderRegistered = false;
    bool FrameRegistered = false;
    bool OptionsRegistered = false;
    bool InputRegistered = false;
    bool AddonEventsRegistered = false;
    // Keep both click coordinates in one atomic value.
    std::atomic<uint64_t> PendingClick{0};
    std::atomic<bool> CanAuthor{false};
    std::atomic<bool> RenderedFrame{false};
    std::atomic<ULONGLONG> AuthorFrameAt{0};
    std::atomic<unsigned> RightButtonUps{0};
    std::atomic<const char*> InputStatus{Tr("No right-click received")};
    const char* AuthorStatus = Tr("Waiting for the map");
    std::atomic<bool> CancelEditor{false};
    bool PopupVisible = false;
    std::atomic<bool> PopupOwnsInput{false};
    bool EditingExisting = false;
    bool EditingFromWorld = false;
    std::string EditingId;
    std::string EditingCharacter;
    uint32_t EditingMapId{};
    Point PendingPoint{};
    float PendingWorldHeight{};
    std::optional<WorldVec3> PendingWorldPosition;
    std::unordered_map<uint32_t, float> WorldHeightReferences;
    Icon PendingIcon = Icon::House;
    std::array<char, 4097> PendingName{};
    ImVec2 EditorAnchor{};
    bool ConfirmDelete = false;
    std::string PendingFolderId;
    bool FolderManagerVisible = false;
    bool SettingsVisible = false;
    ImVec2 EditorWindowPos{};
    ImVec2 EditorWindowSize{};
    ImVec2 SettingsWindowSize{440.0f, 430.0f};
    ImVec2 FolderWindowSize{800.0f, 560.0f};
    std::string ManagerFolderId;
    std::string DeleteFolderId;
    std::string DeleteTargetFolderId;
    std::string DeleteMarkerId;
    std::string SelectedManagerMarkerId;
    std::array<char, 64> NewFolderName{};
    std::array<char, 64> RenameFolderName{};
    std::vector<char> FolderPasteText = std::vector<char>(1024 * 1024 + 1, 0);
    bool PasteVisible = false;
    std::array<char, 1025> PasteText{};
    std::array<char, 257> SettingsPasteText{};
    std::string EditorMessage;
    std::string ManagerMessage;
    std::string SettingsShareMessage;
    std::optional<ShareableFolder> PendingFolderImport;
    std::string PendingFolderImportTarget;
    std::string MinimapDeleteId;
    ImVec2 MinimapDeleteAnchor{};
    size_t WorldBeaconCandidates = 0;
    size_t WorldBeaconsDrawn = 0;
    WorldReferenceTracker BeaconReference;
    RaidMode RaidTools;
    std::atomic<bool> PendingRaidToggle{false};
    std::string HoveredRaidMarker;
    std::atomic<RTAPI::RealTimeData*> RealTimeApi{nullptr};
    std::atomic<bool> PendingAdvancedPlacement{false};
    std::atomic<bool> PendingSettingsToggle{false};
    std::atomic<bool> AdvancedModeEnabled{false};
    std::atomic<bool> HotkeysEnabled{false};
    enum class ShortcutCapture : uint8_t { None, Placement, Settings };
    std::atomic<ShortcutCapture> CapturingShortcut{ShortcutCapture::None};
    std::atomic<uint32_t> CapturedShortcut{0};
    std::atomic<uint32_t> ActivePlacementShortcut{0};
    std::atomic<uint32_t> ActiveSettingsShortcut{0};
    std::string AdvancedStatus;
    std::string ShortcutMessage;

    constexpr std::array<const char*, 18> IconNames{
        "House", "Swords", "Pickaxe", "Mission", "Star", "Chest", "Flag", "Check",
        "Portal", "Arrow Cart", "Catapult", "Trebuchet", "Flame Ram", "Shield Generator",
        "Support", "Damage", "Tank", "Heal"};
    HMODULE Module = nullptr;
    Texture_t* LauncherTexture = nullptr;
    constexpr const char* LauncherTextureId = "VEKKS_ATLAS_152_LAUNCHER_COMPASS";
    constexpr const char* QuickAccessId = "VEKKS_ATLAS_QUICK_ACCESS";
    constexpr const char* QuickAccessBindId = "VEKKS_ATLAS_OPEN_SETTINGS_INTERNAL";
    bool QuickAccessBindRegistered = false;
    bool QuickAccessRegistered = false;
    std::array<Texture_t*, 18> IconTextures128{}; // owned by Nexus
    std::array<Texture_t*, 18> IconTextures64{};
    constexpr std::array<const char*, 18> TextureIds128{
        "VEKKS_ATLAS_113_HOUSE_128", "VEKKS_ATLAS_113_SWORDS_128", "VEKKS_ATLAS_113_PICKAXE_128", "VEKKS_ATLAS_113_MISSION_128",
        "VEKKS_ATLAS_113_STAR_128", "VEKKS_ATLAS_113_CHEST_128", "VEKKS_ATLAS_113_FLAG_128", "VEKKS_ATLAS_113_CHECK_128",
        "VEKKS_ATLAS_140_PORTAL_128", "VEKKS_ATLAS_140_ARROW_CART_128", "VEKKS_ATLAS_140_CATAPULT_128",
        "VEKKS_ATLAS_140_TREBUCHET_128", "VEKKS_ATLAS_140_FLAME_RAM_128", "VEKKS_ATLAS_140_SHIELD_GENERATOR_128",
        "VEKKS_ATLAS_RAID_SUPPORT_128", "VEKKS_ATLAS_RAID_DAMAGE_128", "VEKKS_ATLAS_RAID_TANK_128", "VEKKS_ATLAS_RAID_HEAL_128"};
    constexpr std::array<const char*, 18> TextureIds64{
        "VEKKS_ATLAS_113_HOUSE_64", "VEKKS_ATLAS_113_SWORDS_64", "VEKKS_ATLAS_113_PICKAXE_64", "VEKKS_ATLAS_113_MISSION_64",
        "VEKKS_ATLAS_113_STAR_64", "VEKKS_ATLAS_113_CHEST_64", "VEKKS_ATLAS_113_FLAG_64", "VEKKS_ATLAS_113_CHECK_64",
        "VEKKS_ATLAS_140_PORTAL_64", "VEKKS_ATLAS_140_ARROW_CART_64", "VEKKS_ATLAS_140_CATAPULT_64",
        "VEKKS_ATLAS_140_TREBUCHET_64", "VEKKS_ATLAS_140_FLAME_RAM_64", "VEKKS_ATLAS_140_SHIELD_GENERATOR_64",
        "VEKKS_ATLAS_RAID_SUPPORT_64", "VEKKS_ATLAS_RAID_DAMAGE_64", "VEKKS_ATLAS_RAID_TANK_64", "VEKKS_ATLAS_RAID_HEAL_64"};

    bool Finite(float value) { return std::isfinite(value); }
    bool IsWvWMap(Mumble::EMapType type);
    uint32_t PackShortcut(const Shortcut& shortcut)
    {
        return shortcut.key | (shortcut.alt ? 1u << 16 : 0) | (shortcut.ctrl ? 1u << 17 : 0) |
            (shortcut.shift ? 1u << 18 : 0);
    }

    Shortcut UnpackShortcut(uint32_t packed)
    {
        return {static_cast<uint16_t>(packed & 0xffff), (packed & (1u << 16)) != 0,
            (packed & (1u << 17)) != 0, (packed & (1u << 18)) != 0};
    }

    void SyncShortcutState()
    {
        ActivePlacementShortcut.store(PackShortcut(AtlasPreferences.placementShortcut), std::memory_order_release);
        ActiveSettingsShortcut.store(PackShortcut(AtlasPreferences.settingsShortcut), std::memory_order_release);
    }

    void OnQuickAccessAction(const char*, bool isRelease)
    {
        if (!isRelease) PendingSettingsToggle.store(true, std::memory_order_release);
    }

    bool QuickAccessAvailable()
    {
        return Api && Api->QuickAccess_Add && Api->QuickAccess_Remove &&
            Api->InputBinds_RegisterWithString && Api->InputBinds_Deregister;
    }

    void SyncQuickAccess()
    {
        if (!QuickAccessAvailable()) return;
        if (!QuickAccessBindRegistered)
        {
            Api->InputBinds_RegisterWithString(QuickAccessBindId, OnQuickAccessAction, "");
            QuickAccessBindRegistered = true;
        }
        if (AtlasPreferences.launcherVisible && !QuickAccessRegistered)
        {
            Api->QuickAccess_Add(QuickAccessId, LauncherTextureId, LauncherTextureId,
                QuickAccessBindId, Tr("Open Vekk's Atlas settings"));
            QuickAccessRegistered = true;
        }
        else if (!AtlasPreferences.launcherVisible && QuickAccessRegistered)
        {
            Api->QuickAccess_Remove(QuickAccessId);
            QuickAccessRegistered = false;
        }
    }
    bool Valid(const View& view)
    {
        return view.valid && Finite(view.left) && Finite(view.top) && Finite(view.width) && Finite(view.height) &&
            Finite(view.centerX) && Finite(view.centerY) && Finite(view.scale) && Finite(view.rotation) && view.width > 0 && view.height > 0 && view.scale > 0;
    }

    bool Contains(const View& view, Point point)
    {
        return Valid(view) && point.x >= view.left && point.y >= view.top &&
            point.x <= view.left + view.width && point.y <= view.top + view.height;
    }

    std::optional<Point> ToScreen(const View& view, Point continent)
    {
        if (!Valid(view) || !Finite(continent.x) || !Finite(continent.y)) return std::nullopt;
        const float dx = (continent.x - view.centerX) / view.scale;
        const float dy = (continent.y - view.centerY) / view.scale;
        const float c = std::cos(view.rotation), s = std::sin(view.rotation);
        const Point screen{view.left + view.width * .5f + dx*c - dy*s,
                           view.top + view.height * .5f + dx*s + dy*c};
        if (!Finite(screen.x) || !Finite(screen.y)) return std::nullopt;
        return screen;
    }

    std::optional<Point> ToContinent(const View& view, Point screen)
    {
        if (!Contains(view, screen)) return std::nullopt;
        const float dx = (screen.x - view.left - view.width * .5f) * view.scale;
        const float dy = (screen.y - view.top - view.height * .5f) * view.scale;
        const float c = std::cos(view.rotation), s = std::sin(view.rotation);
        const Point result{view.centerX + dx*c + dy*s, view.centerY - dx*s + dy*c};
        if (!Finite(result.x) || !Finite(result.y)) return std::nullopt;
        return result;
    }

    std::string WideToUtf8(const wchar_t* text, size_t capacity)
    {
        if (!text || !*text) return {};
        const size_t length = wcsnlen_s(text, capacity);
        if (length == capacity) return {};
        const int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, static_cast<int>(length), nullptr, 0, nullptr, nullptr);
        if (bytes <= 0) return {};
        std::string result(static_cast<size_t>(bytes), '\0');
        if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, static_cast<int>(length), result.data(), bytes, nullptr, nullptr) != bytes) return {};
        return result;
    }

    struct IdentitySnapshot
    {
        std::string character;
        float verticalFov{1.22173048f};
    };

    IdentitySnapshot ReadIdentity(const Mumble::Data& data)
    {
        IdentitySnapshot result;
        try
        {
            const std::string identity = WideToUtf8(data.Identity, std::size(data.Identity));
            if (identity.empty()) return result;
            const auto value = json::parse(identity, nullptr, false);
            if (!value.is_object()) return result;
            if (value.contains("name") && value["name"].is_string()) result.character = value["name"].get<std::string>();
            if (value.contains("fov") && value["fov"].is_number())
            {
                const float fov = value["fov"].get<float>();
                if (Finite(fov) && fov >= 0.20f && fov <= 2.80f) result.verticalFov = fov;
            }
        }
        catch (...) {}
        return result;
    }

    State ReadState()
    {
        State state{};
        if (!MumbleLink || !NexusLink) return state;
        state.reason = Tr("Nexus reports no active gameplay");
        if (!NexusLink->IsGameplay) return state;
        Mumble::Data data{};
        bool consistent = false;
        for (int attempt = 0; attempt < 3; ++attempt)
        {
            const unsigned before = reinterpret_cast<volatile Mumble::Data*>(MumbleLink)->UITick;
            MemoryBarrier();
            // The optional Mumble description is not used.
            std::memcpy(&data, MumbleLink, offsetof(Mumble::Data, Context) + sizeof(Mumble::Context));
            MemoryBarrier();
            const unsigned after = reinterpret_cast<volatile Mumble::Data*>(MumbleLink)->UITick;
            if (before != 0 && before == after && data.UITick == before) { consistent = true; break; }
        }
        state.reason = Tr("Inconsistent MumbleLink snapshot");
        if (!consistent) return state;
        state.reason = Tr("Unsupported MumbleLink version");
        if (data.UIVersion != 2) return state;
        state.reason = Tr("MumbleLink belongs to another GW2 process");
        if (data.Context.ProcessID != GetCurrentProcessId()) return state;
        static unsigned lastTick = 0;
        static auto lastUpdate = std::chrono::steady_clock::now();
        const auto now = std::chrono::steady_clock::now();
        if (data.UITick != lastTick) { lastTick = data.UITick; lastUpdate = now; }
        state.reason = Tr("MumbleLink has not updated for 2 seconds");
        if (now - lastUpdate > std::chrono::seconds(2)) return state;
        const auto& context = data.Context;
        state.reason = Tr("GW2 is not focused");
        if (!context.IsGameFocused) return state;
        state.reason = Tr("No active map");
        if (context.MapID == 0) return state;
        const IdentitySnapshot identity = ReadIdentity(data);
        state.character = identity.character;
        state.mapId = context.MapID;
        state.shardId = context.ShardID;
        state.instanceId = context.InstanceID;
        state.mapType = context.MapType;
        state.mapOpen = context.IsMapOpen;
        state.textFocused = context.IsTextboxFocused;
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const float uiScale = NexusLink->Scaling;
        state.reason = Tr("Invalid Nexus scale or display size");
        if (!Finite(uiScale) || uiScale <= 0 || uiScale > 8 || !Finite(display.x) || !Finite(display.y) || display.x <= 0 || display.y <= 0) return state;
        state.uiScale = uiScale;
        state.worldCamera = {
            {data.AvatarPosition.X, data.AvatarPosition.Y, data.AvatarPosition.Z},
            {data.CameraPosition.X, data.CameraPosition.Y, data.CameraPosition.Z},
            {data.CameraFront.X, data.CameraFront.Y, data.CameraFront.Z},
            {data.CameraTop.X, data.CameraTop.Y, data.CameraTop.Z},
            context.Compass.PlayerPosition.X, context.Compass.PlayerPosition.Y,
            identity.verticalFov, display};
        state.avatarValid = Finite(data.AvatarPosition.X) && Finite(data.AvatarPosition.Y) && Finite(data.AvatarPosition.Z) &&
            std::abs(data.AvatarPosition.X) <= 100000.0f && std::abs(data.AvatarPosition.Y) <= 100000.0f && std::abs(data.AvatarPosition.Z) <= 100000.0f;
        const float scale = context.Compass.Scale / uiScale;
        bool baseValid = Finite(scale) && scale > 0.0001f;
        float mapCenterX = context.Compass.Center.X;
        float mapCenterY = context.Compass.Center.Y;
        float minimapCenterX = context.Compass.Center.X;
        float minimapCenterY = context.Compass.Center.Y;
        const bool competitive = IsWvWMap(state.mapType);
        if (competitive)
        {
            // World-space markers remain usable, but MumbleLink does not publish
            // a live WvW map center. Do not guess either native map's viewport.
            baseValid = false;
            if (const auto player = CompetitivePlayerContinent(state.mapId, state.worldCamera.avatarPosition))
            {
                state.worldCamera.playerContinentX = player->x;
                state.worldCamera.playerContinentY = player->y;
            }
        }
        baseValid = baseValid && Finite(mapCenterX) && Finite(mapCenterY) &&
            Finite(minimapCenterX) && Finite(minimapCenterY);
        const float width = static_cast<float>(context.Compass.Width) * uiScale;
        const float height = static_cast<float>(context.Compass.Height) * uiScale;
        // Compass dimensions are not the fullscreen canvas dimensions.
        state.fullMap = View{0, 0, display.x, display.y,
                             mapCenterX, mapCenterY, scale,
                             baseValid && state.mapOpen};
        const float left = display.x - width;
        const float top = context.IsCompassTopRight ? uiScale : display.y - height - 37.0f * uiScale;
        state.minimap = View{left, top, width, height, minimapCenterX, minimapCenterY, scale,
                             baseValid && !state.mapOpen && width > 0 && height > 0 && width <= display.x && height <= display.y,
                             context.IsCompassRotating ? context.Compass.Rotation : 0.0f};
        state.reason = state.character.empty() ? Tr("MumbleLink provides no character name") :
            competitive ? Tr("WvW markers are available in the 3D world only. Manage them through Folders.") :
            !state.mapOpen ? Tr("Open the world map with M") : !Valid(state.fullMap) ? Tr("Map projection unavailable") : Tr("Map ready");
        return state;
    }

    const char* IconKey(Icon icon)
    {
        switch (icon)
        {
        case Icon::House: return "house"; case Icon::Swords: return "swords"; case Icon::Pickaxe: return "pickaxe";
        case Icon::Mission: return "mission"; case Icon::Star: return "star"; case Icon::Chest: return "treasure";
        case Icon::Flag: return "flag"; case Icon::Check: return "check";
        case Icon::Portal: return "wvw_portal"; case Icon::ArrowCart: return "wvw_arrow_cart";
        case Icon::Catapult: return "wvw_catapult"; case Icon::Trebuchet: return "wvw_trebuchet";
        case Icon::FlameRam: return "wvw_flame_ram"; case Icon::ShieldGenerator: return "wvw_shield_generator";
        case Icon::RaidSupport: return "raid_support"; case Icon::RaidDamage: return "raid_damage";
        case Icon::RaidTank: return "raid_tank"; case Icon::RaidHeal: return "raid_heal";
        }
        return "house";
    }

    Icon IconFromKey(std::string_view key)
    {
        if (key == "chest") return Icon::Mission;
        if (key == "skull") return Icon::Chest;
        for (uint8_t index = 0; index < IconNames.size(); ++index)
        {
            const auto icon = static_cast<Icon>(index);
            if (key == IconKey(icon)) return icon;
        }
        throw std::runtime_error(Tr("Unknown icon; the file will not be changed."));
    }

    constexpr size_t MaxStorageBytes = 16 * 1024 * 1024;
    bool SameLabel(std::string_view a, std::string_view b);
    std::string CleanFolderName(std::string name);

    int64_t ReadInteger(const json& value, int64_t minimum, int64_t maximum)
    {
        // Reject JSON coercion before converting to an integer.
        if (!value.is_number_integer()) throw std::runtime_error(Tr("Expected an integer."));
        if (value.is_number_unsigned())
        {
            const auto number = value.get<uint64_t>();
            if (number > static_cast<uint64_t>(maximum) || (minimum > 0 && number < static_cast<uint64_t>(minimum)))
                throw std::runtime_error(Tr("Integer out of range."));
            return static_cast<int64_t>(number);
        }
        const auto number = value.get<int64_t>();
        if (number < minimum || number > maximum) throw std::runtime_error(Tr("Integer out of range."));
        return number;
    }

    void LoadMapNames()
    {
        MapNames.clear();
        try
        {
            const WORD resourceId = GetLanguage() == Language::Spanish ? 109 : GetLanguage() == Language::German ? 111 : 110;
            const HRSRC resource = FindResourceW(Module, MAKEINTRESOURCEW(resourceId), MAKEINTRESOURCEW(10));
            if (!resource) throw std::runtime_error(Tr("Map catalog not found."));
            const DWORD size = SizeofResource(Module, resource);
            const HGLOBAL loaded = LoadResource(Module, resource);
            const auto* data = loaded ? static_cast<const char*>(LockResource(loaded)) : nullptr;
            if (!data || size == 0) throw std::runtime_error(Tr("Could not open the map catalog."));
            const auto maps = json::parse(data, data + size);
            if (!maps.is_array()) throw std::runtime_error(Tr("Invalid map catalog."));
            for (const auto& item : maps)
            {
                if (!item.is_object() || !item.contains("id") || !item.contains("name") || !item["name"].is_string()) continue;
                const auto id = static_cast<uint32_t>(ReadInteger(item["id"], 1, UINT32_MAX));
                auto name = item["name"].get<std::string>();
                if (name.empty() || !ValidLabel(name, 256, false)) continue;
                MapNames.insert_or_assign(id, std::move(name));
            }
            if (MapNames.empty()) throw std::runtime_error(Tr("The map catalog is empty."));
        }
        catch (const std::exception& error)
        {
            MapNames.clear();
            if (Api && Api->Log) Api->Log(LOGL_WARNING, AddonName, error.what());
        }
    }

    const char* MapName(uint32_t mapId)
    {
        const auto found = MapNames.find(mapId);
        return found == MapNames.end() ? Tr("Unknown map") : found->second.c_str();
    }

    void MarkDirty()
    {
        if (!Dirty) DirtySince = std::chrono::steady_clock::now();
        Dirty = true;
    }

    ImVec2 ClampWindowPosition(ImVec2 position, ImVec2 size)
    {
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        constexpr float margin = 8.0f;
        position.x = std::clamp(position.x, margin, std::max(margin, display.x - size.x - margin));
        position.y = std::clamp(position.y, margin, std::max(margin, display.y - size.y - margin));
        return position;
    }

    void KeepCurrentWindowOnScreen(ImVec2* savedPosition = nullptr, ImVec2* savedSize = nullptr)
    {
        const ImVec2 size = ImGui::GetWindowSize();
        const ImVec2 position = ClampWindowPosition(ImGui::GetWindowPos(), size);
        ImGui::SetWindowPos(position, ImGuiCond_Always);
        if (savedPosition) *savedPosition = position;
        if (savedSize) *savedSize = size;
    }

    ImVec2 AttachedWindowPosition(ImVec2 auxiliarySize, float scale)
    {
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const float gap = 8.0f * scale;
        ImVec2 desired{};
        if (EditorWindowPos.x + EditorWindowSize.x + gap + auxiliarySize.x <= display.x - 8.0f)
            desired = {EditorWindowPos.x + EditorWindowSize.x + gap, EditorWindowPos.y};
        else
            desired = {EditorWindowPos.x - auxiliarySize.x - gap, EditorWindowPos.y};
        return ClampWindowPosition(desired, auxiliarySize);
    }

    std::string NewId()
    {
        static std::mt19937_64 engine{std::random_device{}()};
        const auto now = static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
        return std::to_string(now) + "-" + std::to_string(engine());
    }

    void SaveMarkers()
    {
        if (!Dirty || DataFile.empty() || !StorageWritable) return;
        DirtySince = std::chrono::steady_clock::now();
        try
        {
        json root{{"schema_version", 4}, {"raids", RaidTools.Save()}, {"markers", json::array()}, {"folders", json::array()}, {"unfiled_visibility", json::array()},
            {"settings", {{"language", LanguageCode(AtlasPreferences.language)}, {"map_icon_size", AtlasPreferences.mapIconSize},
                {"minimap_icon_size", AtlasPreferences.minimapIconSize}, {"label_scale", AtlasPreferences.labelScale},
                {"name_map", AtlasPreferences.nameMap}, {"name_minimap", AtlasPreferences.nameMinimap}, {"name_world", AtlasPreferences.nameWorld},
                {"distance_map", AtlasPreferences.distanceMap}, {"distance_minimap", AtlasPreferences.distanceMinimap}, {"distance_world", AtlasPreferences.distanceWorld},
                {"icon_opacity", AtlasPreferences.iconOpacity}, {"name_opacity", AtlasPreferences.nameOpacity}, {"distance_opacity", AtlasPreferences.distanceOpacity},
                {"edge_indicators", AtlasPreferences.edgeIndicators}, {"icon_effects", AtlasPreferences.iconEffects},
                {"label_background", AtlasPreferences.labelBackground}, {"share_help", AtlasPreferences.shareHelp},
                {"world_beacons", AtlasPreferences.worldBeacons}, {"beacon_opacity", AtlasPreferences.beaconOpacity},
                {"beacon_max_distance", AtlasPreferences.beaconMaxDistance},
                {"beacon_width", AtlasPreferences.beaconWidth},
                {"world_icons", AtlasPreferences.worldIcons},
                {"world_icon_scale", AtlasPreferences.worldIconScale}, {"world_text_scale", AtlasPreferences.worldTextScale},
                {"world_icon_opacity", AtlasPreferences.worldIconOpacity}, {"world_name_opacity", AtlasPreferences.worldNameOpacity},
                {"world_distance_opacity", AtlasPreferences.worldDistanceOpacity},
                {"advanced_mode", AtlasPreferences.advancedMode},
                {"launcher_visible", AtlasPreferences.launcherVisible}, {"launcher_size", AtlasPreferences.launcherSize},
                {"launcher_x", AtlasPreferences.launcherX}, {"launcher_y", AtlasPreferences.launcherY},
                {"placement_shortcut_key", AtlasPreferences.placementShortcut.key},
                {"placement_shortcut_alt", AtlasPreferences.placementShortcut.alt},
                {"placement_shortcut_ctrl", AtlasPreferences.placementShortcut.ctrl},
                {"placement_shortcut_shift", AtlasPreferences.placementShortcut.shift},
                {"settings_shortcut_key", AtlasPreferences.settingsShortcut.key},
                {"settings_shortcut_alt", AtlasPreferences.settingsShortcut.alt},
                {"settings_shortcut_ctrl", AtlasPreferences.settingsShortcut.ctrl},
                {"settings_shortcut_shift", AtlasPreferences.settingsShortcut.shift}}}};
        for (const auto& folder : Folders)
            root["folders"].push_back({{"id", folder.id}, {"character", folder.character}, {"name", folder.name}, {"visible", folder.visible}});
        for (const auto& [character, visible] : UnfiledVisibility)
            root["unfiled_visibility"].push_back({{"character", character}, {"visible", visible}});
        for (const auto& marker : Markers)
        {
            root["markers"].push_back({
                {"id", marker.id}, {"character", marker.character}, {"map_id", marker.mapId},
                {"continent_id", marker.continentId}, {"floor_id", marker.floorId},
                {"continent_position", {{"x", marker.continent.x}, {"y", marker.continent.y}}},
                {"icon", IconKey(marker.icon)}, {"name", marker.name}, {"folder_id", marker.folderId}});
            if (marker.worldHeight) root["markers"].back()["world_height"] = *marker.worldHeight;
            if (marker.worldPosition) root["markers"].back()["world_position"] = {
                {"x", marker.worldPosition->x}, {"y", marker.worldPosition->y}, {"z", marker.worldPosition->z}};
        }
            const auto serialized = root.dump(2);
            if (serialized.size() > MaxStorageBytes)
                throw std::runtime_error(Tr("Data exceeds 16 MiB. Shorten names or remove markers before closing the game; the previous file is intact."));
            std::filesystem::create_directories(DataFile.parent_path());
            if (!SchemaOneSource.empty() && std::filesystem::exists(SchemaOneSource))
            {
                auto originalBackup = SchemaOneSource;
                originalBackup += L".v1.bak";
                if (!std::filesystem::exists(originalBackup) && !CopyFileW(SchemaOneSource.c_str(), originalBackup.c_str(), TRUE))
                    throw std::runtime_error(Tr("Could not back up the previous version; migration has been postponed."));
            }
            auto temporary = DataFile;
            temporary += L".tmp";
            std::ofstream output(temporary, std::ios::trunc | std::ios::binary);
            output << serialized;
            output.flush();
            const bool written = output.good();
            output.close();
            if (!written || output.fail()) throw std::runtime_error(Tr("Could not write the temporary file."));
            auto backup = DataFile;
            backup += L".bak";
            if (std::filesystem::exists(DataFile) && !CopyFileW(DataFile.c_str(), backup.c_str(), FALSE))
                throw std::runtime_error(Tr("Could not create a backup; the original file is preserved."));
            if (!MoveFileExW(temporary.c_str(), DataFile.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                throw std::runtime_error(Tr("Could not replace markers.json; changes remain in memory."));
            Dirty = false;
            SchemaOneSource.clear();
            StorageError.clear();
        }
        catch (const std::exception& error)
        {
            if (StorageError != error.what() && Api && Api->Log) Api->Log(LOGL_WARNING, AddonName, error.what());
            StorageError = error.what();
        }
    }

    void LoadMarkers()
    {
        RaidTools.Reset();
        Markers.clear();
        Folders.clear();
        UnfiledVisibility.clear();
        AtlasPreferences = Preferences{};
        SetLanguage(Language::English);
        Preferences loadedPreferences{};
        SchemaOneSource.clear();
        StorageWritable = true;
        StorageError.clear();
        try
        {
            if (DataFile.empty() || !std::filesystem::exists(DataFile)) return;
            if (std::filesystem::file_size(DataFile) > MaxStorageBytes) throw std::runtime_error(Tr("Marker file is too large; it will not be changed."));
            std::ifstream input(DataFile);
            json root; input >> root;
            if (!root.is_object() || !root.contains("schema_version") || !root.contains("markers") || !root["markers"].is_array())
                throw std::runtime_error(Tr("Unknown marker format; the file will not be changed."));
            const int schema = static_cast<int>(ReadInteger(root.at("schema_version"), 1, 4));
            if (root.contains("settings"))
            {
                const auto& settings = root.at("settings");
                if (!settings.is_object()) throw std::runtime_error(Tr("Invalid settings; the file will not be changed."));
                if (settings.contains("language"))
                {
                    if (!settings["language"].is_string()) throw std::runtime_error(Tr("Invalid settings; the file will not be changed."));
                    loadedPreferences.language = LanguageFromCode(settings["language"].get<std::string>());
                    SetLanguage(loadedPreferences.language);
                }
                if (settings.contains("label_mode"))
                {
                    const int mode = static_cast<int>(ReadInteger(settings.at("label_mode"), 0, 3));
                    loadedPreferences.nameMap = mode == 0 || mode == 1;
                    loadedPreferences.nameMinimap = mode == 0 || mode == 2;
                }
                const auto readFloat = [&](const char* key, float minimum, float maximum, float& target)
                {
                    if (!settings.contains(key)) return;
                    const auto& value = settings.at(key);
                    if (!value.is_number()) throw std::runtime_error(Tr("Invalid numeric setting."));
                    const float number = value.get<float>();
                    if (!Finite(number) || number < minimum || number > maximum) throw std::runtime_error(Tr("Numeric setting out of range."));
                    target = number;
                };
                const auto readBool = [&](const char* key, bool& target)
                {
                    if (!settings.contains(key)) return;
                    const auto& value = settings.at(key);
                    if (!value.is_boolean()) throw std::runtime_error(Tr("Invalid boolean setting."));
                    target = value.get<bool>();
                };
                const auto readShortcut = [&](const char* prefix, Shortcut& target)
                {
                    const std::string keyName = std::string(prefix) + "_key";
                    const std::string altName = std::string(prefix) + "_alt";
                    const std::string ctrlName = std::string(prefix) + "_ctrl";
                    const std::string shiftName = std::string(prefix) + "_shift";
                    if (settings.contains(keyName)) target.key = static_cast<uint16_t>(ReadInteger(settings.at(keyName), 0, UINT16_MAX));
                    readBool(altName.c_str(), target.alt);
                    readBool(ctrlName.c_str(), target.ctrl);
                    readBool(shiftName.c_str(), target.shift);
                    if (IsModifierKey(target.key)) throw std::runtime_error(Tr("A shortcut cannot use only a modifier key."));
                };
                readFloat("map_icon_size", 20.0f, 72.0f, loadedPreferences.mapIconSize);
                readFloat("minimap_icon_size", 16.0f, 56.0f, loadedPreferences.minimapIconSize);
                readFloat("label_scale", .70f, 1.60f, loadedPreferences.labelScale);
                readFloat("icon_opacity", .10f, 1.00f, loadedPreferences.iconOpacity);
                readFloat("name_opacity", .10f, 1.00f, loadedPreferences.nameOpacity);
                readFloat("distance_opacity", .10f, 1.00f, loadedPreferences.distanceOpacity);
                readFloat("beacon_opacity", .10f, 1.00f, loadedPreferences.beaconOpacity);
                readFloat("beacon_max_distance", WorldUnitsToMetres(250.0f), 5000.0f, loadedPreferences.beaconMaxDistance);
                readFloat("beacon_width", .25f, 3.0f, loadedPreferences.beaconWidth);
                readFloat("world_icon_scale", .50f, 2.00f, loadedPreferences.worldIconScale);
                readFloat("world_text_scale", .70f, 1.60f, loadedPreferences.worldTextScale);
                if (settings.contains("world_icon_opacity")) readFloat("world_icon_opacity", .10f, 1.00f, loadedPreferences.worldIconOpacity);
                else loadedPreferences.worldIconOpacity = loadedPreferences.iconOpacity;
                if (settings.contains("world_name_opacity")) readFloat("world_name_opacity", .10f, 1.00f, loadedPreferences.worldNameOpacity);
                else loadedPreferences.worldNameOpacity = loadedPreferences.nameOpacity;
                if (settings.contains("world_distance_opacity")) readFloat("world_distance_opacity", .10f, 1.00f, loadedPreferences.worldDistanceOpacity);
                else loadedPreferences.worldDistanceOpacity = loadedPreferences.distanceOpacity;
                readFloat("launcher_size", 24.0f, 96.0f, loadedPreferences.launcherSize);
                readFloat("launcher_x", -10000.0f, 10000.0f, loadedPreferences.launcherX);
                readFloat("launcher_y", -10000.0f, 10000.0f, loadedPreferences.launcherY);
                readBool("edge_indicators", loadedPreferences.edgeIndicators);
                readBool("icon_effects", loadedPreferences.iconEffects);
                readBool("label_background", loadedPreferences.labelBackground);
                readBool("share_help", loadedPreferences.shareHelp);
                readBool("name_map", loadedPreferences.nameMap);
                readBool("name_minimap", loadedPreferences.nameMinimap);
                readBool("name_world", loadedPreferences.nameWorld);
                readBool("distance_map", loadedPreferences.distanceMap);
                readBool("distance_minimap", loadedPreferences.distanceMinimap);
                readBool("distance_world", loadedPreferences.distanceWorld);
                readBool("world_beacons", loadedPreferences.worldBeacons);
                readBool("world_icons", loadedPreferences.worldIcons);
                if (settings.contains("world_names")) readBool("world_names", loadedPreferences.nameWorld);
                readBool("advanced_mode", loadedPreferences.advancedMode);
                readBool("launcher_visible", loadedPreferences.launcherVisible);
                readShortcut("placement_shortcut", loadedPreferences.placementShortcut);
                readShortcut("settings_shortcut", loadedPreferences.settingsShortcut);
            }
            if (schema >= 2)
            {
                if (!root.contains("folders") || !root["folders"].is_array() || root["folders"].size() > 1000)
                    throw std::runtime_error(Tr("Invalid folder list; the file will not be changed."));
                for (const auto& item : root["folders"])
                {
                    Folder folder{item.at("id").get<std::string>(), item.at("character").get<std::string>(), item.at("name").get<std::string>(), item.value("visible", true)};
                    if (!ValidLabel(folder.id, 256, false) || !ValidLabel(folder.character, 256, false) ||
                        CleanFolderName(folder.name) != folder.name ||
                        std::any_of(Folders.begin(), Folders.end(), [&](const Folder& other) { return other.id == folder.id || (other.character == folder.character && SameLabel(other.name, folder.name)); }))
                        throw std::runtime_error(Tr("Invalid or duplicate folder; the file will not be changed."));
                    Folders.push_back(std::move(folder));
                }
            }
            if (schema >= 3 && root.contains("unfiled_visibility"))
            {
                if (!root["unfiled_visibility"].is_array() || root["unfiled_visibility"].size() > 1000)
                    throw std::runtime_error(Tr("Invalid folder visibility list; the file will not be changed."));
                for (const auto& item : root["unfiled_visibility"])
                {
                    if (!item.is_object() || !item.contains("character") || !item["character"].is_string() ||
                        !item.contains("visible") || !item["visible"].is_boolean())
                        throw std::runtime_error(Tr("Invalid folder visibility list; the file will not be changed."));
                    const auto character = item["character"].get<std::string>();
                    if (!ValidLabel(character, 256, false))
                        throw std::runtime_error(Tr("Invalid folder visibility list; the file will not be changed."));
                    UnfiledVisibility.insert_or_assign(character, item["visible"].get<bool>());
                }
            }
            if (root["markers"].size() > 20000) throw std::runtime_error(Tr("Too many markers; the file will not be changed."));
            for (const auto& item : root["markers"])
            {
                try
                {
                    if (!item.is_object() || !item.contains("id") || !item.contains("character") || !item.contains("map_id") ||
                        !item.contains("continent_position") || !item["continent_position"].is_object()) throw std::runtime_error(Tr("Incomplete marker."));
                    Marker marker{};
                    marker.id = item.at("id").get<std::string>(); marker.character = item.at("character").get<std::string>();
                    marker.mapId = static_cast<uint32_t>(ReadInteger(item.at("map_id"), 1, UINT32_MAX));
                    marker.continentId = item.contains("continent_id") ? static_cast<uint32_t>(ReadInteger(item.at("continent_id"), 0, UINT32_MAX)) : 0;
                    marker.floorId = item.contains("floor_id") ? static_cast<int32_t>(ReadInteger(item.at("floor_id"), INT32_MIN, INT32_MAX)) : 0;
                    marker.name = item.value("name", "");
                    marker.folderId = schema >= 2 ? item.value("folder_id", "") : "";
                    marker.icon = IconFromKey(item.value("icon", "house"));
                    if (item.contains("world_position"))
                    {
                        const auto& position = item.at("world_position");
                        if (!position.is_object() || !position.at("x").is_number() ||
                            !position.at("y").is_number() || !position.at("z").is_number())
                            throw std::runtime_error(Tr("Invalid world position."));
                        const WorldVec3 world{position.at("x").get<float>(), position.at("y").get<float>(), position.at("z").get<float>()};
                        if (!Finite(world.x) || !Finite(world.y) || !Finite(world.z) ||
                            std::abs(world.x) > 100000.0f || std::abs(world.y) > 100000.0f || std::abs(world.z) > 100000.0f)
                            throw std::runtime_error(Tr("Invalid world position."));
                        marker.worldPosition = world;
                    }
                    if (item.contains("world_height"))
                    {
                        if (!item["world_height"].is_number()) throw std::runtime_error(Tr("World elevation must be numeric."));
                        const float height = item["world_height"].get<float>();
                        if (!Finite(height) || std::abs(height) > 100000.0f) throw std::runtime_error(Tr("Invalid world elevation."));
                        marker.worldHeight = height;
                    }
                    if (!item["continent_position"].at("x").is_number() || !item["continent_position"].at("y").is_number())
                        throw std::runtime_error(Tr("Coordinates must be numeric."));
                    marker.continent.x = item["continent_position"].at("x").get<float>();
                    marker.continent.y = item["continent_position"].at("y").get<float>();
                    if (!ValidLabel(marker.id, 256, false) || !ValidLabel(marker.character, 256, false) ||
                        !ValidLabel(marker.name, 4096) || !Finite(marker.continent.x) || !Finite(marker.continent.y) ||
                        std::any_of(Markers.begin(), Markers.end(), [&](const Marker& other) { return other.id == marker.id; }) ||
                        (!marker.folderId.empty() && !std::any_of(Folders.begin(), Folders.end(), [&](const Folder& folder) { return folder.id == marker.folderId && folder.character == marker.character; })))
                        throw std::runtime_error(Tr("Invalid or duplicate marker."));
                    Markers.push_back(std::move(marker));
                }
                catch (...) { throw std::runtime_error(Tr("The file contains invalid markers; it is preserved and writing is disabled.")); }
            }
            if (root.contains("raids")) RaidTools.Load(root.at("raids"));
            if (schema == 1) SchemaOneSource = DataFile;
            AtlasPreferences = loadedPreferences;
            SetLanguage(AtlasPreferences.language);
        }
        catch (const std::exception& error)
        {
            Markers.clear();
            RaidTools.Reset();
            Folders.clear();
            UnfiledVisibility.clear();
            StorageWritable = false;
            StorageError = error.what();
            if (Api && Api->Log) Api->Log(LOGL_WARNING, AddonName, error.what());
        }
    }

    ImU32 IconColor(Icon icon)
    {
        return static_cast<ImU32>(MarkerColor(icon));
    }

    ImU32 ColorWithAlpha(ImU32 color, int alpha)
    {
        return (color & ~IM_COL32_A_MASK) |
            (static_cast<ImU32>(std::clamp(alpha, 0, 255)) << IM_COL32_A_SHIFT);
    }

    void RefreshTextures()
    {
        if (!Api || !Api->Textures_Get) return;
        if (!LauncherTexture) LauncherTexture = Api->Textures_Get(LauncherTextureId);
        for (size_t i = 0; i < IconTextures128.size(); ++i)
        {
            if (!IconTextures128[i]) IconTextures128[i] = Api->Textures_Get(TextureIds128[i]);
            if (!IconTextures64[i]) IconTextures64[i] = Api->Textures_Get(TextureIds64[i]);
        }
    }

    void DrawIcon(ImDrawList* draw, ImVec2 center, float radius, Icon icon, float opacity = 1.0f)
    {
        const auto index = static_cast<size_t>(icon);
        if (index >= IconTextures128.size()) return;
        Texture_t* texture = radius * 2.0f <= 48.0f ? IconTextures64[index] : IconTextures128[index];
        if (!texture || !texture->Resource)
            texture = radius * 2.0f <= 48.0f ? IconTextures128[index] : IconTextures64[index];
        if (texture && texture->Resource)
        {
            const ImVec2 minimum{center.x-radius, center.y-radius};
            const ImVec2 maximum{center.x+radius, center.y+radius};
            if (AtlasPreferences.iconEffects)
            {
                const float offset = std::clamp(radius * 0.09f, 1.0f, 3.0f);
                draw->AddImage(texture->Resource,
                    {minimum.x + offset, minimum.y + offset}, {maximum.x + offset, maximum.y + offset},
                    {0,0}, {1,1}, IM_COL32(0, 0, 0, static_cast<int>(150 * std::clamp(opacity, 0.0f, 1.0f))));
            }
            draw->AddImage(texture->Resource, minimum, maximum, {0,0}, {1,1},
                IM_COL32(255, 255, 255, static_cast<int>(255 * std::clamp(opacity, 0.0f, 1.0f))));
        }
    }

    bool IsWvWMap(Mumble::EMapType type)
    {
        switch (type)
        {
        case Mumble::EMapType::WvW_EternalBattlegrounds:
        case Mumble::EMapType::WvW_BlueBorderlands:
        case Mumble::EMapType::WvW_GreenBorderlands:
        case Mumble::EMapType::WvW_RedBorderlands:
        case Mumble::EMapType::WVW_FortunesVale:
        case Mumble::EMapType::WvW_ObsidianSanctum:
        case Mumble::EMapType::WvW_EdgeOfTheMists:
        case Mumble::EMapType::WvW_Lounge: return true;
        default: return false;
        }
    }

    bool MarkerFolderVisible(const Marker& marker)
    {
        if (marker.folderId.empty())
        {
            const auto found = UnfiledVisibility.find(marker.character);
            return found == UnfiledVisibility.end() || found->second;
        }
        const auto folder = std::find_if(Folders.begin(), Folders.end(), [&](const Folder& candidate) {
            return candidate.character == marker.character && candidate.id == marker.folderId;
        });
        return folder != Folders.end() && folder->visible;
    }

    bool MarkerApplies(const Marker& marker, const State& state)
    {
        return marker.character == state.character && marker.mapId == state.mapId && MarkerFolderVisible(marker);
    }

    std::optional<Point> MarkerMapPosition(const Marker& marker)
    {
        if (marker.worldPosition)
            if (const auto point=CompetitivePlayerContinent(marker.mapId, *marker.worldPosition)) return point;
        return marker.continent;
    }

    std::string FormatDistance(const Marker& marker, const State& state)
    {
        const float dx = marker.continent.x - state.worldCamera.playerContinentX;
        const float dy = marker.continent.y - state.worldCamera.playerContinentY;
        const float distance = std::sqrt(dx * dx + dy * dy);
        char text[32]{};
        if (distance >= 10000.0f) snprintf(text, sizeof(text), "%.1fk u", distance / 1000.0f);
        else snprintf(text, sizeof(text), "%.0f u", distance);
        return text;
    }

    float ReferenceWorldHeight(const State& state)
    {
        // Keep fallback elevation fixed for the session.
        const auto existing = WorldHeightReferences.find(state.mapId);
        if (existing != WorldHeightReferences.end()) return existing->second;
        if (!state.mapId || !IsWorldBeaconCameraValid(state.worldCamera)) return 0.0f;
        const float height = state.worldCamera.avatarPosition.y;
        if (!Finite(height) || std::abs(height) > 100000.0f) return 0.0f;
        return WorldHeightReferences.try_emplace(state.mapId, height).first->second;
    }

    RTAPI::RealTimeData* RefreshRealTimeApi()
    {
        auto* data = RealTimeApi.load(std::memory_order_acquire);
        if ((!data || data->GameBuild == 0) && Api && Api->DataLink_Get)
        {
            data = static_cast<RTAPI::RealTimeData*>(Api->DataLink_Get(DL_RTAPI));
            RealTimeApi.store(data, std::memory_order_release);
        }
        return data;
    }

    bool RealTimeApiReady()
    {
        auto* data = RefreshRealTimeApi();
        return data && data->GameBuild != 0;
    }

    std::optional<RTAPI::RealTimeData> ReadRealTimeApi()
    {
        auto* source = RefreshRealTimeApi();
        if (!source) return std::nullopt;
        const uint32_t before = source->GameBuild;
        if (before == 0) return std::nullopt;
        MemoryBarrier();
        RTAPI::RealTimeData snapshot{};
        std::memcpy(&snapshot, source, sizeof(snapshot));
        MemoryBarrier();
        if (source->GameBuild != before || snapshot.GameBuild != before) return std::nullopt;
        return snapshot;
    }

    void OnAddonLoaded(void* payload)
    {
        if (!payload || *static_cast<uint32_t*>(payload) != RTAPI_SIG || !Api || !Api->DataLink_Get) return;
        RealTimeApi.store(static_cast<RTAPI::RealTimeData*>(Api->DataLink_Get(DL_RTAPI)), std::memory_order_release);
    }

    void OnAddonUnloaded(void* payload)
    {
        if (payload && *static_cast<uint32_t*>(payload) == RTAPI_SIG)
            RealTimeApi.store(nullptr, std::memory_order_release);
    }

    const char* AdvancedErrorText(AdvancedPlacementError error)
    {
        switch (error)
        {
        case AdvancedPlacementError::StaleData: return Tr("RTAPI data is stale. Reload RTAPI and try again.");
        case AdvancedPlacementError::NotGameplay: return Tr("RTAPI is not reporting active gameplay.");
        case AdvancedPlacementError::MapMismatch: return Tr("RTAPI and MumbleLink are reporting different maps. Try again after loading finishes.");
        case AdvancedPlacementError::InvalidReference: return Tr("RTAPI and MumbleLink could not align their world coordinates.");
        case AdvancedPlacementError::InvalidCursor: return Tr("Move the cursor over a valid point in the 3D world and try again.");
        default: return Tr("The 3D cursor position is unavailable.");
        }
    }

    void DrawWorldMarkerBeacons(const State& state)
    {
        HoveredRaidMarker.clear();
        WorldBeaconCandidates = 0;
        WorldBeaconsDrawn = 0;
        if (!AtlasPreferences.worldBeacons || state.mapOpen || state.character.empty() ||
            !IsWorldBeaconCameraValid(state.worldCamera)) return;
        // Keep one fixed map/world anchor. Session metadata can change while the
        // character moves, but that must not make saved beacons follow them.
        WorldMapReference reference{};
        std::vector<WorldBeacon> beacons;
        beacons.reserve(Markers.size() + 1);
        const float referenceHeight = ReferenceWorldHeight(state);
        for (const auto& marker : Markers)
            if (MarkerApplies(marker, state))
            {
                if (!marker.worldPosition)
                    reference = BeaconReference.GetOrCreate(state.mapId, state.character, state.worldCamera);
                const auto index = static_cast<size_t>(marker.icon);
                auto* texture = index < IconTextures128.size() ? IconTextures128[index] : nullptr;
                if ((!texture || !texture->Resource) && index < IconTextures64.size()) texture = IconTextures64[index];
                beacons.push_back({marker.continent.x, marker.continent.y, IconColor(marker.icon),
                    marker.worldHeight.value_or(marker.worldPosition ? marker.worldPosition->y : referenceHeight),
                    texture ? texture->Resource : nullptr, marker.name, marker.worldPosition, marker.id});
            }
        if (PopupVisible && EditingFromWorld && !EditingExisting && state.character == EditingCharacter && state.mapId == EditingMapId)
        {
            if (!PendingWorldPosition)
                reference = BeaconReference.GetOrCreate(state.mapId, state.character, state.worldCamera);
            const auto index = static_cast<size_t>(PendingIcon);
            auto* texture = index < IconTextures128.size() ? IconTextures128[index] : nullptr;
            if ((!texture || !texture->Resource) && index < IconTextures64.size()) texture = IconTextures64[index];
            beacons.push_back({PendingPoint.x, PendingPoint.y, IconColor(PendingIcon), PendingWorldHeight,
                texture ? texture->Resource : nullptr, PendingName.data(), PendingWorldPosition});
        }
        if (beacons.empty()) return;
        WorldBeaconCandidates = beacons.size();
        WorldBeaconsDrawn = DrawWorldBeacons(ImGui::GetBackgroundDrawList(), state.worldCamera, reference, beacons,
            {AtlasPreferences.beaconOpacity, AtlasPreferences.beaconMaxDistance, state.uiScale,
                AtlasPreferences.worldIcons, AtlasPreferences.nameWorld, AtlasPreferences.distanceWorld,
                AtlasPreferences.worldIconOpacity, AtlasPreferences.worldNameOpacity, AtlasPreferences.worldDistanceOpacity,
                AtlasPreferences.worldIconScale, AtlasPreferences.worldTextScale, AtlasPreferences.beaconWidth},
            RaidTools.enabled && RaidTools.eraser ? &HoveredRaidMarker : nullptr);
    }

    std::string DisplayLabel(std::string_view name)
    {
        if (name.size() <= 96) return std::string(name);
        size_t count = 93;
        while (count > 0 && (static_cast<unsigned char>(name[count]) & 0xc0) == 0x80) --count;
        std::string label(name.substr(0, count));
        label += "...";
        return label;
    }

    void DrawMarkerLabel(ImDrawList* draw, const View& view, ImVec2 center, float radius,
        std::string_view name, std::string_view distance, bool showName, bool showDistance)
    {
        showName = showName && !name.empty();
        showDistance = showDistance && !distance.empty();
        if (!showName && !showDistance) return;
        const std::string nameLabel = showName ? DisplayLabel(name) : std::string{};
        const std::string separator = showName && showDistance ? "  ·  " : "";
        const std::string distanceLabel = showDistance ? std::string(distance) : std::string{};
        const std::string label = nameLabel + separator + distanceLabel;
        ImFont* font = ImGui::GetFont();
        const float fontSize = ImGui::GetFontSize() * AtlasPreferences.labelScale;
        const ImVec2 size = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label.c_str());
        float x = center.x - size.x * .5f;
        float y = center.y + radius + 3.0f;
        x = std::clamp(x, view.left + 3.0f, std::max(view.left + 3.0f, view.left + view.width - size.x - 3.0f));
        if (y + size.y + 3.0f > view.top + view.height) y = center.y - radius - size.y - 3.0f;
        const ImVec2 pos{x,y};
        if (AtlasPreferences.labelBackground)
            draw->AddRectFilled({x-4.0f,y-2.0f},{x+size.x+4.0f,y+size.y+2.0f},IM_COL32(3,6,8,165),3.0f);
        const auto drawPart = [&](const std::string& text, float& textX, float opacity, ImU32 color)
        {
            if (text.empty()) return;
            const int foregroundAlpha = static_cast<int>(255 * std::clamp(opacity, .1f, 1.0f));
            const int outlineAlpha = static_cast<int>(235 * std::clamp(opacity, .1f, 1.0f));
            const ImU32 outline = IM_COL32(0,0,0,outlineAlpha);
            draw->AddText(font,fontSize,{textX-1.0f,y},outline,text.c_str());
            draw->AddText(font,fontSize,{textX+1.0f,y},outline,text.c_str());
            draw->AddText(font,fontSize,{textX,y-1.0f},outline,text.c_str());
            draw->AddText(font,fontSize,{textX,y+1.0f},outline,text.c_str());
            draw->AddText(font,fontSize,{textX,y},ColorWithAlpha(color, foregroundAlpha),text.c_str());
            textX += font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text.c_str()).x;
        };
        float textX = pos.x;
        drawPart(nameLabel, textX, AtlasPreferences.nameOpacity, IM_COL32(248,244,226,255));
        drawPart(separator, textX, std::max(AtlasPreferences.nameOpacity, AtlasPreferences.distanceOpacity), IM_COL32(181,190,190,255));
        drawPart(distanceLabel, textX, AtlasPreferences.distanceOpacity, IM_COL32(203,226,229,255));
    }

    void DrawMarkers(const State& state, const View& view, bool minimap)
    {
        if (!Valid(view) || state.character.empty() || Markers.empty()) return;
        // Keep markers below Nexus windows and inside the map.
        auto* draw = ImGui::GetBackgroundDrawList();
        draw->PushClipRect({view.left, view.top}, {view.left + view.width, view.top + view.height}, true);
        const Point mouse{ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y};
        const float radius = .5f * (minimap ? AtlasPreferences.minimapIconSize : AtlasPreferences.mapIconSize) * state.uiScale;
        for (const auto& marker : Markers)
        {
            if (!MarkerApplies(marker, state)) continue;
            const auto point = MarkerMapPosition(marker);
            if (!point) continue;
            auto screen = ToScreen(view, *point);
            if (!screen) continue;
            bool pinned = false;
            if (minimap)
            {
                const float halfWidth = view.width * .5f - radius - 2.0f * state.uiScale;
                const float halfHeight = view.height * .5f - radius - 2.0f * state.uiScale;
                if (halfWidth <= 0 || halfHeight <= 0) continue;
                const float cx = view.left + view.width * .5f, cy = view.top + view.height * .5f;
                const float dx = screen->x - cx, dy = screen->y - cy;
                const float boundary = std::max(std::abs(dx) / halfWidth, std::abs(dy) / halfHeight);
                if (boundary > 1.0f)
                {
                    if (!AtlasPreferences.edgeIndicators) continue;
                    // Intersect the direction ray with the minimap edge.
                    screen = Point{cx + dx / boundary, cy + dy / boundary};
                    pinned = true;
                }
            }
            else if (!Contains(view, *screen)) continue;
            const ImVec2 center{screen->x, screen->y};
            const ImU32 markerColor = IconColor(marker.icon);
            if (AtlasPreferences.iconEffects)
            {
                const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(ImGui::GetTime()) * 2.7f);
                const float innerRadius = radius * (1.04f + 0.055f * pulse);
                const float outerRadius = innerRadius + (2.5f + 1.5f * pulse) * state.uiScale;
                draw->AddCircle(center, outerRadius, ColorWithAlpha(markerColor, static_cast<int>(35 + 30 * pulse)), 32, 3.0f * state.uiScale);
                draw->AddCircle(center, innerRadius, ColorWithAlpha(markerColor, static_cast<int>(115 + 85 * pulse)), 32, 1.8f * state.uiScale);
            }
            DrawIcon(draw, center, radius, marker.icon, AtlasPreferences.iconOpacity);
            const bool showName = minimap ? AtlasPreferences.nameMinimap : AtlasPreferences.nameMap;
            const bool showDistance = minimap ? AtlasPreferences.distanceMinimap : AtlasPreferences.distanceMap;
            DrawMarkerLabel(draw, view, center, radius, marker.name, FormatDistance(marker, state), showName, showDistance);
            const float dx = mouse.x - screen->x, dy = mouse.y - screen->y;
            if (Contains(view, mouse) && !PopupVisible && !ImGui::GetIO().WantCaptureMouse && dx*dx + dy*dy <= radius*radius)
            {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted(marker.name.empty() ? Tr(IconNames[static_cast<size_t>(marker.icon)]) : marker.name.c_str());
                if (pinned) ImGui::TextDisabled(Tr("Outside the minimap: this indicates its direction"));
                ImGui::EndTooltip();
            }
        }
        draw->PopClipRect();
    }

    void DrawPendingMarker(const State& state)
    {
        if (!PopupVisible || EditingExisting || !state.mapOpen || state.character != EditingCharacter ||
            state.mapId != EditingMapId || !Valid(state.fullMap)) return;

        const auto point = PendingWorldPosition ? std::optional<Point>{CompetitivePlayerContinent(EditingMapId, *PendingWorldPosition).value_or(PendingPoint)} : std::optional<Point>{PendingPoint};
        if (!point) return;
        const auto screen = ToScreen(state.fullMap, *point);
        if (!screen || !Contains(state.fullMap, *screen)) return;

        const float scale = state.uiScale;
        const float radius = .5f * AtlasPreferences.mapIconSize * scale;
        const float pulse = .5f + .5f * std::sin(static_cast<float>(ImGui::GetTime()) * 4.0f);
        const float ring = radius * (1.34f + .09f * pulse);
        const ImVec2 center{screen->x, screen->y};
        auto* draw = ImGui::GetBackgroundDrawList();
        draw->PushClipRect({state.fullMap.left, state.fullMap.top},
            {state.fullMap.left + state.fullMap.width, state.fullMap.top + state.fullMap.height}, true);

        const ImU32 softGold = IM_COL32(238, 197, 105, static_cast<int>(72 + 46 * pulse));
        const ImU32 solidGold = IM_COL32(255, 220, 140, static_cast<int>(190 + 55 * pulse));
        draw->AddCircle(center, ring + 3.0f * scale, softGold, 32, 3.0f * scale);
        draw->AddCircle(center, ring, solidGold, 32, 1.5f * scale);
        const float inner = ring + 2.0f * scale;
        const float outer = ring + 7.0f * scale;
        draw->AddLine({center.x - outer, center.y}, {center.x - inner, center.y}, solidGold, 1.5f * scale);
        draw->AddLine({center.x + inner, center.y}, {center.x + outer, center.y}, solidGold, 1.5f * scale);
        draw->AddLine({center.x, center.y - outer}, {center.x, center.y - inner}, solidGold, 1.5f * scale);
        draw->AddLine({center.x, center.y + inner}, {center.x, center.y + outer}, solidGold, 1.5f * scale);
        DrawIcon(draw, center, radius, PendingIcon);
        draw->PopClipRect();
    }

    Marker* FindMarker(std::string_view id)
    {
        const auto found = std::find_if(Markers.begin(), Markers.end(), [&](const Marker& marker) { return marker.id == id; });
        return found == Markers.end() ? nullptr : &*found;
    }

    const Folder* FindFolder(std::string_view id, std::string_view character)
    {
        const auto found = std::find_if(Folders.begin(), Folders.end(), [&](const Folder& folder) {
            return folder.id == id && folder.character == character;
        });
        return found == Folders.end() ? nullptr : &*found;
    }

    const char* FolderName(std::string_view id, std::string_view character)
    {
        const auto* folder = FindFolder(id, character);
        return folder ? folder->name.c_str() : Tr("Unfiled");
    }

    bool SameLabel(std::string_view a, std::string_view b)
    {
        wchar_t wa[128]{}, wb[128]{};
        const int na = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, a.data(), static_cast<int>(a.size()), wa, 128);
        const int nb = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, b.data(), static_cast<int>(b.size()), wb, 128);
        return na > 0 && nb > 0 && CompareStringOrdinal(wa, na, wb, nb, TRUE) == CSTR_EQUAL;
    }

    std::string CleanFolderName(std::string name)
    {
        const auto first = name.find_first_not_of(' ');
        if (first == std::string::npos) throw std::runtime_error(Tr("Enter a folder name."));
        name = name.substr(first, name.find_last_not_of(' ') - first + 1);
        if (!ValidLabel(name, 63, false) || SameLabel(name, "sin directorio"))
            throw std::runtime_error(Tr("Invalid or reserved folder name (maximum 63 UTF-8 bytes)."));
        return name;
    }

    template<size_t N> void CopyText(std::array<char, N>& target, std::string_view text)
    {
        target.fill(0);
        size_t count = std::min(text.size(), N - 1);
        while (count < text.size() && count > 0 && (static_cast<unsigned char>(text[count]) & 0xc0) == 0x80) --count;
        std::copy_n(text.data(), count, target.data());
    }

    template<typename Action> void UIAction(std::string& message, Action&& action)
    {
        try
        {
            if (!StorageWritable) throw std::runtime_error(Tr("Data protected: resolve the save error before changing markers."));
            message.clear();
            action();
        }
        catch (const std::exception& ex) { message = ex.what(); }
    }

    bool FolderSelector(const char* label, const std::string& character, std::string& selected, const std::string& excluded = {})
    {
        if (!selected.empty() && (!FindFolder(selected, character) || selected == excluded)) selected.clear();
        bool changed = false;
        if (ImGui::BeginCombo(label, FolderName(selected, character)))
        {
            EndCombo end;
            if (ImGui::Selectable(UiLabel("Unfiled").c_str(), selected.empty())) { selected.clear(); changed = true; }
            for (const auto& folder : Folders)
            {
                if (folder.character != character || folder.id == excluded) continue;
                ScopedID id(folder.id.c_str());
                // Hidden ID keeps literal ## valid in folder names.
                const auto pos = ImGui::GetCursorScreenPos();
                if (ImGui::Selectable("##folder", selected == folder.id)) { selected = folder.id; changed = true; }
                ImGui::GetWindowDrawList()->AddText(pos, ImGui::GetColorU32(ImGuiCol_Text), folder.name.c_str());
            }
        }
        return changed;
    }

    std::string CreateFolder(const std::string& character, const std::string& rawName)
    {
        const auto name = CleanFolderName(rawName);
        if (Folders.size() >= 1000) throw std::runtime_error(Tr("The 1,000-folder limit has been reached."));
        for (const auto& folder : Folders)
            if (folder.character == character && SameLabel(folder.name, name)) throw std::runtime_error(Tr("A folder with that name already exists."));
        const auto id = NewId();
        Folders.push_back({id, character, name});
        MarkDirty();
        return id;
    }

    void RenameFolder(const std::string& character, const std::string& id, const std::string& rawName)
    {
        const auto name = CleanFolderName(rawName);
        for (const auto& folder : Folders)
            if (folder.character == character && folder.id != id && SameLabel(folder.name, name)) throw std::runtime_error(Tr("A folder with that name already exists."));
        auto found = std::find_if(Folders.begin(), Folders.end(), [&](const Folder& folder) { return folder.id == id && folder.character == character; });
        if (found == Folders.end()) throw std::runtime_error(Tr("Select a folder to rename."));
        found->name = name;
        MarkDirty();
    }

    size_t FolderCount(std::string_view character, std::string_view folderId)
    {
        return static_cast<size_t>(std::count_if(Markers.begin(), Markers.end(), [&](const Marker& marker) {
            return marker.character == character && marker.folderId == folderId;
        }));
    }

    void SelectManagerFolder(const std::string& id)
    {
        ManagerFolderId = id;
        CopyText(RenameFolderName, id.empty() ? "" : FolderName(id, EditingCharacter));
        DeleteFolderId.clear(); DeleteTargetFolderId.clear(); DeleteMarkerId.clear(); ManagerMessage.clear();
        SelectedManagerMarkerId.clear();
    }

    void RemoveFolder(bool deleteContents)
    {
        if (DeleteFolderId.empty() || !FindFolder(DeleteFolderId, EditingCharacter)) throw std::runtime_error(Tr("The folder no longer exists."));
        if (!deleteContents && (DeleteTargetFolderId == DeleteFolderId ||
            (!DeleteTargetFolderId.empty() && !FindFolder(DeleteTargetFolderId, EditingCharacter))))
            throw std::runtime_error(Tr("Choose a different destination folder."));
        // Apply folder changes together.
        auto updatedMarkers = Markers;
        auto updatedFolders = Folders;
        const std::string target = deleteContents ? "" : DeleteTargetFolderId;
        std::string nextPending = PendingFolderId == DeleteFolderId ? target : PendingFolderId;
        std::string nextSelected = target;
        if (deleteContents)
            updatedMarkers.erase(std::remove_if(updatedMarkers.begin(), updatedMarkers.end(), [&](const Marker& marker) {
                return marker.character == EditingCharacter && marker.folderId == DeleteFolderId;
            }), updatedMarkers.end());
        else
            for (auto& marker : updatedMarkers)
                if (marker.character == EditingCharacter && marker.folderId == DeleteFolderId) marker.folderId = target;
        updatedFolders.erase(std::remove_if(updatedFolders.begin(), updatedFolders.end(), [&](const Folder& folder) {
            return folder.character == EditingCharacter && folder.id == DeleteFolderId;
        }), updatedFolders.end());
        Markers.swap(updatedMarkers); Folders.swap(updatedFolders);
        PendingFolderId.swap(nextPending); ManagerFolderId.swap(nextSelected);
        DeleteFolderId.clear(); DeleteTargetFolderId.clear(); DeleteMarkerId.clear();
        CopyText(RenameFolderName, FolderName(ManagerFolderId, EditingCharacter));
        if (EditingExisting && !FindMarker(EditingId)) { PopupVisible = false; FolderManagerVisible = false; SettingsVisible = false; }
        MarkDirty();
    }

    void RemoveMarker(const std::string& id, std::string_view character = {})
    {
        if (character.empty()) character = EditingCharacter;
        Markers.erase(std::remove_if(Markers.begin(), Markers.end(), [&](const Marker& marker) {
            return marker.id == id && marker.character == character;
        }), Markers.end());
        if (EditingExisting && EditingId == id) { PopupVisible = false; FolderManagerVisible = false; SettingsVisible = false; }
        MarkDirty();
    }

    void CopyMarkerLink(const Marker& marker, std::string& message)
    {
        std::string link, error;
        const auto* folder = FindFolder(marker.folderId, marker.character);
        ShareableMarker shared{marker.mapId, marker.continentId, marker.floorId, marker.continent, marker.icon,
            marker.name, folder ? folder->name : "", marker.worldHeight, marker.worldPosition};
        if (!EncodeMarkerLink(shared, link, error) || !CopyLinkToClipboard(link, error)) throw std::runtime_error(error);
        message = Tr("Marker code copied to clipboard (") + std::to_string(link.size()) + "/256).";
    }

    void CopyFolderLink(const std::string& folderId, const std::string& character, std::string& message)
    {
        const auto* folder = FindFolder(folderId, character);
        if (!folder) throw std::runtime_error(Tr("Select a folder to copy."));
        ShareableFolder shared;
        shared.name = folder->name;
        for (const auto& marker : Markers)
            if (marker.character == character && marker.folderId == folderId)
                shared.markers.push_back({marker.mapId, marker.continentId, marker.floorId, marker.continent,
                    marker.icon, marker.name, {}, marker.worldHeight, marker.worldPosition});
        std::string code, error;
        if (!EncodeFolderLink(shared, code, error) || !CopyLinkToClipboard(code, error)) throw std::runtime_error(error);
        message = Tr("Folder copied: ") + std::to_string(shared.markers.size()) + Tr(" markers in the folder code.");
    }

    void LoadFolderCodeFromClipboard()
    {
        std::string text, error;
        if (!ReadLinkFromClipboard(text, error)) throw std::runtime_error(error);
        if (!text.starts_with("VAF1:") && !text.starts_with("VAF2:") && !text.starts_with("VAF3:"))
            throw std::runtime_error(Tr("The clipboard does not contain a Vekk's Atlas folder code."));
        if (text.size() >= FolderPasteText.size()) throw std::runtime_error(Tr("The folder code exceeds the 1 MiB limit."));
        std::fill(FolderPasteText.begin(), FolderPasteText.end(), char{0});
        std::copy(text.begin(), text.end(), FolderPasteText.begin());
    }

    std::string UniqueFolderName(const std::string& character, const std::string& base)
    {
        for (unsigned suffix = 2; suffix < 10000; ++suffix)
        {
            const std::string tail = " (" + std::to_string(suffix) + ")";
            size_t count = std::min(base.size(), 63 - tail.size());
            while (count < base.size() && count > 0 && (static_cast<unsigned char>(base[count]) & 0xC0) == 0x80) --count;
            std::string candidate = base.substr(0, count);
            candidate += tail;
            const bool exists = std::any_of(Folders.begin(), Folders.end(), [&](const Folder& folder) {
                return folder.character == character && SameLabel(folder.name, candidate);
            });
            if (!exists) return candidate;
        }
        throw std::runtime_error(Tr("Could not create a unique folder name."));
    }

    void ApplyFolderImport(const State& state, const ShareableFolder& shared, std::string replaceId, bool forceNew)
    {
        std::string folderName = CleanFolderName(shared.name);
        if (forceNew) folderName = UniqueFolderName(state.character, folderName);
        auto updatedFolders = Folders;
        auto updatedMarkers = Markers;
        std::string target = std::move(replaceId);
        if (!target.empty())
        {
            auto folder = std::find_if(updatedFolders.begin(), updatedFolders.end(), [&](const Folder& item) {
                return item.character == state.character && item.id == target;
            });
            if (folder == updatedFolders.end()) throw std::runtime_error(Tr("The folder no longer exists."));
            folder->name = folderName;
            updatedMarkers.erase(std::remove_if(updatedMarkers.begin(), updatedMarkers.end(), [&](const Marker& marker) {
                return marker.character == state.character && marker.folderId == target;
            }), updatedMarkers.end());
        }
        else
        {
            if (updatedFolders.size() >= 1000) throw std::runtime_error(Tr("Cannot import folder: folder limit reached."));
            target = NewId();
            updatedFolders.push_back({target, state.character, folderName, true});
        }
        size_t imported = 0;
        for (const auto& incoming : shared.markers)
        {
            if (updatedMarkers.size() >= 20000) throw std::runtime_error(Tr("Cannot import folder: the 20,000-marker limit has been reached."));
            updatedMarkers.push_back({NewId(), state.character, incoming.mapId, incoming.continentId, incoming.floorId,
                incoming.continent, incoming.icon, incoming.name, target, incoming.worldHeight, incoming.worldPosition});
            ++imported;
        }
        Folders.swap(updatedFolders); Markers.swap(updatedMarkers);
        ManagerFolderId = target; PendingFolderId = target;
        std::fill(FolderPasteText.begin(), FolderPasteText.end(), char{0});
        PendingFolderImport.reset(); PendingFolderImportTarget.clear();
        MarkDirty(); SaveMarkers();
        if (Dirty) throw std::runtime_error(StorageError.empty() ? Tr("Could not save the imported folder.") : StorageError);
        ManagerMessage = Tr("Folder imported: ") + std::to_string(imported) + Tr(" markers.");
    }

    void ImportFolderLink(const State& state)
    {
        const size_t length = strnlen_s(FolderPasteText.data(), FolderPasteText.size());
        if (length == FolderPasteText.size()) throw std::runtime_error(Tr("The pasted code is not properly terminated."));
        ShareableFolder shared;
        std::string error;
        if (!DecodeFolderLink(std::string_view(FolderPasteText.data(), length), shared, error)) throw std::runtime_error(error);
        const std::string folderName = CleanFolderName(shared.name);
        const auto conflict = std::find_if(Folders.begin(), Folders.end(), [&](const Folder& folder) {
            return folder.character == state.character && SameLabel(folder.name, folderName);
        });
        if (conflict != Folders.end())
        {
            PendingFolderImport = std::move(shared);
            PendingFolderImportTarget = conflict->id;
            ManagerMessage = Tr("A folder with this name already exists. Replace it or create a separate copy.");
            return;
        }
        ApplyFolderImport(state, shared, {}, false);
    }

    void ImportMarkerLink(const State& state)
    {
        ShareableMarker shared;
        std::string error;
        if (!DecodeMarkerLink(PasteText.data(), shared, error)) throw std::runtime_error(error);
        if (Markers.size() >= 20000) throw std::runtime_error(Tr("The 20,000-marker limit has been reached."));
        for (const auto& marker : Markers)
            if (marker.character == state.character && marker.mapId == shared.mapId && marker.continentId == shared.continentId &&
                marker.floorId == shared.floorId && marker.continent.x == shared.continent.x && marker.continent.y == shared.continent.y &&
                marker.icon == shared.icon && marker.name == shared.name)
                throw std::runtime_error(Tr("This marker already exists for your character. No duplicate was created."));
        auto updatedFolders = Folders;
        auto updatedMarkers = Markers;
        std::string target;
        if (!shared.folderName.empty())
        {
            const auto folderName = CleanFolderName(shared.folderName);
            for (const auto& folder : updatedFolders)
                if (folder.character == state.character && SameLabel(folder.name, folderName)) { target = folder.id; break; }
            if (target.empty())
            {
                if (updatedFolders.size() >= 1000) throw std::runtime_error(Tr("Cannot import folder: limit reached."));
                target = NewId();
                updatedFolders.push_back({target, state.character, folderName});
            }
        }
        updatedMarkers.push_back({NewId(), state.character, shared.mapId, shared.continentId, shared.floorId,
            shared.continent, shared.icon, shared.name, target, shared.worldHeight, shared.worldPosition});
        Markers.swap(updatedMarkers); Folders.swap(updatedFolders);
        MarkDirty(); SaveMarkers();
        PopupVisible = false; FolderManagerVisible = false; SettingsVisible = false;
        if (Api && Api->GUI_SendAlert) Api->GUI_SendAlert(Dirty ? Tr("Marker imported, but saving failed. Check Atlas options.") :
            shared.mapId == state.mapId ? Tr("Vekk's Atlas: marker imported.") : Tr("Vekk's Atlas: marker imported. It will appear when you enter its map."));
    }

    std::optional<std::string> HitMarker(const State& state, const View& view, Point click, bool minimap)
    {
        if (!Contains(view, click)) return std::nullopt;
        const float radius = .5f * (minimap ? AtlasPreferences.minimapIconSize : AtlasPreferences.mapIconSize) * state.uiScale;
        for (const auto& marker : Markers)
        {
            if (!MarkerApplies(marker, state)) continue;
            const auto point = MarkerMapPosition(marker);
            if (!point) continue;
            auto screen = ToScreen(view, *point);
            if (!screen) continue;
            if (minimap)
            {
                const float halfWidth = view.width * .5f - radius - 2.0f * state.uiScale;
                const float halfHeight = view.height * .5f - radius - 2.0f * state.uiScale;
                if (halfWidth <= 0 || halfHeight <= 0) continue;
                const float cx = view.left + view.width * .5f, cy = view.top + view.height * .5f;
                const float dx = screen->x - cx, dy = screen->y - cy;
                const float boundary = std::max(std::abs(dx) / halfWidth, std::abs(dy) / halfHeight);
                if (boundary > 1.0f)
                {
                    if (!AtlasPreferences.edgeIndicators) continue;
                    screen = Point{cx + dx / boundary, cy + dy / boundary};
                }
            }
            else if (!Contains(view, *screen)) continue;
            const float dx = click.x - screen->x, dy = click.y - screen->y;
            if (dx*dx + dy*dy <= radius*radius) return marker.id;
        }
        return std::nullopt;
    }

    void DrawMinimapDelete(const State& state)
    {
        if (MinimapDeleteId.empty()) return;
        auto* marker = FindMarker(MinimapDeleteId);
        if (!marker || marker->character != state.character)
        { MinimapDeleteId.clear(); return; }
        const float scale = std::clamp(state.uiScale, .5f, 4.0f);
        AtlasTheme theme(scale);
        ImGui::SetNextWindowPos(ClampWindowPosition(MinimapDeleteAnchor, {280.0f * scale, 120.0f * scale}), ImGuiCond_Appearing);
        ImGui::SetNextWindowSize({280.0f * scale, 0}, ImGuiCond_Appearing);
        bool open = true;
        if (ImGui::Begin(UiLabel("Marker options###AtlasMinimapMarker").c_str(), &open,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings))
        {
            ImGui::TextWrapped(Tr("Delete '%s' from the minimap?"),
                marker->name.empty() ? Tr(IconNames[static_cast<size_t>(marker->icon)]) : marker->name.c_str());
            if (GoldButton(UiLabel("Delete marker").c_str()))
            {
                Markers.erase(std::remove_if(Markers.begin(), Markers.end(), [&](const Marker& item) {
                    return item.id == MinimapDeleteId && item.character == state.character;
                }), Markers.end());
                MarkDirty(); MinimapDeleteId.clear(); open = false;
            }
            ImGui::SameLine();
            if (ImGui::Button(UiLabel("Cancel##minimapdelete").c_str()))
            { MinimapDeleteId.clear(); open = false; }
            KeepCurrentWindowOnScreen();
        }
        ImGui::End();
        if (!open) MinimapDeleteId.clear();
    }

    void OpenEditor(const State& state, Point continent, const std::optional<std::string>& existing,
        std::optional<float> capturedWorldHeight = std::nullopt, bool fromWorld = false)
    {
        PendingPoint = continent;
        PendingWorldPosition.reset();
        PendingWorldHeight = capturedWorldHeight.value_or(state.worldCamera.avatarPosition.y);
        if (!Finite(PendingWorldHeight)) PendingWorldHeight = ReferenceWorldHeight(state);
        EditingExisting = existing.has_value();
        EditingFromWorld = fromWorld;
        EditingId = existing.value_or("");
        EditingCharacter = state.character;
        EditingMapId = state.mapId;
        PendingIcon = Icon::House;
        ConfirmDelete = false;
        PendingFolderId.clear();
        EditorMessage.clear(); ManagerMessage.clear();
        PasteVisible = false; PasteText.fill(0); FolderManagerVisible = false; SettingsVisible = false;
        CancelEditor.store(false);
        EditorAnchor = ImGui::GetIO().MousePos;
        PendingName.fill('\0');
        if (auto* marker = existing ? FindMarker(*existing) : nullptr)
        {
            PendingIcon = marker->icon;
            PendingPoint = marker->continent;
            PendingWorldPosition = marker->worldPosition;
            PendingWorldHeight = marker->worldHeight.value_or(marker->worldPosition ? marker->worldPosition->y : ReferenceWorldHeight(state));
            PendingFolderId = marker->folderId;
            size_t copied = std::min(marker->name.size(), PendingName.size() - 1);
            while (copied < marker->name.size() && copied > 0 && (static_cast<unsigned char>(marker->name[copied]) & 0xC0) == 0x80) --copied;
            std::copy_n(marker->name.data(), copied, PendingName.data());
        }
        PopupVisible = true;
        HotkeysEnabled.store(false, std::memory_order_release);
    }

    void ProcessAdvancedPlacement(const State& state)
    {
        if (!PendingAdvancedPlacement.exchange(false, std::memory_order_acq_rel)) return;
        const auto report = [&](const char* message)
        {
            AdvancedStatus = message;
            if (Api && Api->GUI_SendAlert) Api->GUI_SendAlert(AdvancedStatus.c_str());
        };
        if (!StorageWritable) { report(Tr("Saving is blocked by a file error.")); return; }
        if (PopupVisible) { report(Tr("Close the current marker window before placing another marker.")); return; }
        if (state.textFocused) { report(Tr("Close the active text field before placing a 3D marker.")); return; }
        if (state.character.empty() || state.mapId == 0 || !state.avatarValid)
        { report(Tr("MumbleLink is not ready for 3D placement.")); return; }

        if (IsWvWMap(state.mapType))
        {
            const auto position = state.worldCamera.avatarPosition;
            if (!Finite(position.x) || !Finite(position.y) || !Finite(position.z) ||
                std::abs(position.x) > 100000.0f || std::abs(position.y) > 100000.0f || std::abs(position.z) > 100000.0f)
            { report(Tr("MumbleLink is not ready for 3D placement.")); return; }
            // The map coordinate is only a display cache. The beacon keeps this exact world position.
            const Point mapPoint = CompetitivePlayerContinent(state.mapId, position).value_or(Point{});
            OpenEditor(state, mapPoint, std::nullopt, position.y, true);
            PendingWorldPosition = position;
            report(Tr("3D marker ready. Choose its icon and name, then save."));
            return;
        }
        if (!AtlasPreferences.advancedMode) { report(Tr("Advanced mode is disabled in Vekk's Atlas settings.")); return; }
        if (!IsWorldBeaconCameraValid(state.worldCamera))
        { report(Tr("MumbleLink is not ready for 3D placement.")); return; }

        const auto realtime = ReadRealTimeApi();
        if (!realtime)
        {
            report(RefreshRealTimeApi() ? Tr("RTAPI data is stale. Reload RTAPI and try again.") :
                Tr("RTAPI is not installed or is not currently loaded."));
            return;
        }
        const AdvancedCursorSnapshot snapshot{realtime->GameBuild, realtime->MapID,
            realtime->GameState == RTAPI::EGameState::Gameplay,
            {realtime->Cursor[0], realtime->Cursor[1], realtime->Cursor[2]},
            {realtime->CharacterPosition[0], realtime->CharacterPosition[1], realtime->CharacterPosition[2]},
            {realtime->CameraPosition[0], realtime->CameraPosition[1], realtime->CameraPosition[2]},
            {realtime->CameraFacing[0], realtime->CameraFacing[1], realtime->CameraFacing[2]}};
        const WorldMapReference reference{state.worldCamera.avatarPosition,
            state.worldCamera.playerContinentX, state.worldCamera.playerContinentY};
        AdvancedPlacementError error{};
        const auto placement = ConvertAdvancedCursor(snapshot, state.mapId, reference,
            state.worldCamera.cameraPosition, state.worldCamera.cameraFront, error);
        if (!placement) { report(AdvancedErrorText(error)); return; }

        OpenEditor(state, placement->continent, std::nullopt, placement->worldHeight, true);
        report(Tr("3D marker ready. Choose its icon and name, then save."));
    }

    void SavePendingMarker(const State& state)
    {
        if (!StorageWritable || state.character != EditingCharacter || state.mapId != EditingMapId)
            throw std::runtime_error(Tr("MumbleLink is not ready for 3D placement."));
        if (!ValidLabel(PendingName.data(), 4096)) throw std::runtime_error(Tr("Invalid name."));
        if (!Finite(PendingWorldHeight) || std::abs(PendingWorldHeight) > 100000.0f)
            throw std::runtime_error(Tr("Invalid 3D elevation."));
        if (PendingWorldPosition && (!Finite(PendingWorldPosition->x) || !Finite(PendingWorldPosition->y) || !Finite(PendingWorldPosition->z) ||
            std::abs(PendingWorldPosition->x) > 100000.0f || std::abs(PendingWorldPosition->y) > 100000.0f || std::abs(PendingWorldPosition->z) > 100000.0f))
            throw std::runtime_error(Tr("Invalid world position."));
        if (auto* marker = EditingExisting ? FindMarker(EditingId) : nullptr)
        {
            Marker updated = *marker;
            updated.name = PendingName.data(); updated.icon = PendingIcon; updated.folderId = PendingFolderId;
            updated.worldHeight = PendingWorldHeight;
            updated.worldPosition = PendingWorldPosition;
            if (PendingWorldPosition)
                if (const auto point = CompetitivePlayerContinent(updated.mapId, *PendingWorldPosition)) updated.continent = *point;
            *marker = std::move(updated);
        }
        else if (!EditingExisting)
        {
            if (Markers.size() >= 20000) throw std::runtime_error(Tr("The 20,000-marker limit has been reached."));
            auto id = NewId();
            Markers.push_back(Marker{id, EditingCharacter, EditingMapId, 0, 0, PendingPoint, PendingIcon, PendingName.data(), PendingFolderId, PendingWorldHeight, PendingWorldPosition});
            EditingId = std::move(id);
            EditingExisting = true;
        }
        else throw std::runtime_error(Tr("The marker no longer exists."));
        MarkDirty(); SaveMarkers();
        if (Dirty) throw std::runtime_error(StorageError.empty() ? Tr("Could not save; do not close the game.") : StorageError);
        PopupVisible = false; FolderManagerVisible = false; SettingsVisible = false;
    }

    void DrawIconChoice(Icon icon, bool compact = false)
    {
        const bool selected = PendingIcon == icon;
        ImGui::PushID(static_cast<int>(icon));
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const float scale = std::clamp(NexusLink->Scaling, .5f, 4.0f);
        const float side = RaidTools.enabled ? 32.0f : compact ? 42.0f : 62.0f;
        if (ImGui::InvisibleButton("icon", {side*scale, side*scale})) PendingIcon = icon;
        auto* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(pos, {pos.x+side*scale,pos.y+side*scale}, selected ? IM_COL32(65,57,34,255) :
            ImGui::IsItemHovered() ? IM_COL32(41,50,52,255) : IM_COL32(24,33,37,255), 5*scale);
        DrawIcon(draw, {pos.x + side*.5f*scale, pos.y + side*.5f*scale}, (RaidTools.enabled ? 14.0f : compact ? 17.0f : 25.0f)*scale, icon);
        if (selected) draw->AddRect(pos, {pos.x+side*scale,pos.y+side*scale}, IM_COL32(224, 183, 100, 255), 5*scale, 0, 1.5f*scale);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", Tr(IconNames[static_cast<size_t>(icon)]));
        ImGui::PopID();
    }

    void DrawGeneralIconChoices(bool compact)
    {
        constexpr Icon icons[]{Icon::House,Icon::Swords,Icon::Pickaxe,Icon::Mission,Icon::Star,Icon::Chest,Icon::Flag,Icon::Check,
            Icon::RaidSupport,Icon::RaidDamage,Icon::RaidTank,Icon::RaidHeal};
        const int count=RaidTools.enabled ? 12 : 8, columns=RaidTools.enabled ? 6 : 4;
        for (int i=0;i<count;++i) { if (i%columns) ImGui::SameLine(); DrawIconChoice(icons[i],compact); }
    }

    void DrawWvWIconChoices(bool compact)
    {
        ImGui::TextDisabled(Tr("WvW tactical markers"));
        for (uint8_t index = static_cast<uint8_t>(Icon::Portal); index <= static_cast<uint8_t>(Icon::ShieldGenerator); ++index)
        {
            if (index != static_cast<uint8_t>(Icon::Portal)) ImGui::SameLine();
            DrawIconChoice(static_cast<Icon>(index), compact);
        }
    }

    void DrawWorldHeightEditor(const State& state)
    {
        if (!ImGui::TreeNode(UiLabel("World elevation").c_str())) return;
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##WorldHeight", &PendingWorldHeight, .25f, -100000.0f, 100000.0f, Tr("Elevation: %.2f m"));
        if (ImGui::Button(UiLabel("Use my elevation").c_str(), {-1,0})) PendingWorldHeight = state.worldCamera.avatarPosition.y;
        if (PendingWorldPosition)
            ImGui::TextWrapped(Tr("This marker is anchored to your saved MumbleLink position. Elevation can still be adjusted manually."));
        else if (EditingFromWorld && !EditingExisting)
            ImGui::TextWrapped(Tr("This elevation was captured from the RTAPI 3D cursor. You can still adjust it manually."));
        else
            ImGui::TextWrapped(Tr("Stand on the ground next to the marker, click Use my elevation, then save. Distant locations have approximate elevation; terrain is not detected."));
        ImGui::TreePop();
    }

    void DrawQuickEditor(const State& state)
    {
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const float scale = std::clamp(state.uiScale, .5f, 4.0f);
        AtlasTheme theme(scale);
        ImGui::SetNextWindowSize({310*scale, 0}, ImGuiCond_Appearing);
        ImGui::SetNextWindowSizeConstraints({std::min(310*scale,display.x-16),0}, {std::max(100.0f,display.x-16),std::max(100.0f,display.y-16)});
        ImGui::SetNextWindowPos({std::clamp(EditorAnchor.x, 8.0f, std::max(8.0f, display.x - 318*scale)),
                                 std::clamp(EditorAnchor.y, 8.0f, std::max(8.0f, display.y - 500*scale))}, ImGuiCond_Appearing);
        const bool visible = ImGui::Begin(UiLabel("Edit marker###AtlasQuickEditor").c_str(), &PopupVisible,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
        EndWindow window;
        if (!visible) return;
        const auto* saved = FindMarker(EditingId);
        if (!saved) { PopupVisible = false; FolderManagerVisible = false; SettingsVisible = false; return; }

        ImGui::TextColored({.86f,.71f,.40f,1}, Tr("QUICK EDIT"));
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 120*scale);
        if (FolderButton(scale))
        {
            FolderManagerVisible = !FolderManagerVisible;
            if (FolderManagerVisible) { SettingsVisible = false; SelectManagerFolder(PendingFolderId); }
        }
        ImGui::SameLine();
        if (SettingsButton(scale)) { SettingsVisible = !SettingsVisible; if (SettingsVisible) FolderManagerVisible = false; }
        ImGui::SameLine(); if (RaidTools.Button(scale)) MarkDirty();
        ImGui::TextDisabled("%s", MapName(saved->mapId));
        ImGui::Separator();
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##QuickName", PendingName.data(), PendingName.size());
        DrawGeneralIconChoices(true);
        if (IsWvWMap(state.mapType)) DrawWvWIconChoices(true);
        ImGui::SetNextItemWidth(-1);
        FolderSelector("##QuickFolder", state.character, PendingFolderId);
        if (IsWvWMap(state.mapType) && ImGui::TreeNode(UiLabel("World position").c_str()))
        {
            if (PendingWorldPosition)
            {
                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat("##WorldX", &PendingWorldPosition->x, .25f, -100000.0f, 100000.0f, "X: %.2f m");
                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat("##WorldZ", &PendingWorldPosition->z, .25f, -100000.0f, 100000.0f, "Z: %.2f m");
            }
            if (ImGui::Button(UiLabel("Use my position").c_str(), {-1,0}) && state.avatarValid)
            {
                PendingWorldPosition = state.worldCamera.avatarPosition;
                PendingWorldHeight = PendingWorldPosition->y;
            }
            ImGui::TreePop();
        }
        DrawWorldHeightEditor(state);
        if (ImGui::Button(UiLabel("Close").c_str(), {90*scale,0})) { PopupVisible = false; FolderManagerVisible = false; SettingsVisible = false; }
        ImGui::SameLine();
        if (GoldButton(UiLabel("Save changes").c_str(), {-1,0})) UIAction(EditorMessage, [&] { SavePendingMarker(state); });
        ImGui::Separator();
        if (ImGui::Button(UiLabel("Copy Marker").c_str(), {-1,0})) UIAction(EditorMessage, [&] {
            if (const auto* marker = FindMarker(EditingId)) CopyMarkerLink(*marker, EditorMessage);
            else throw std::runtime_error(Tr("The marker no longer exists."));
        });
        if (AtlasPreferences.shareHelp)
        {
            if (ImGui::TreeNodeEx(UiLabel("How to share a marker").c_str(), ImGuiTreeNodeFlags_SpanAvailWidth))
            {
                ImGui::TextWrapped(Tr("Copy the marker code and send it to a friend. Pasting it into Vekk's Atlas recreates the same location, name and icon."));
                ImGui::TreePop();
            }
        }
        if (!ConfirmDelete)
        {
            if (ImGui::Button(UiLabel("Delete").c_str(), {-1,0})) ConfirmDelete = true;
        }
        else
        {
            ImGui::TextWrapped(Tr("Delete this marker?"));
            if (ImGui::Button(UiLabel("Yes, delete").c_str())) UIAction(EditorMessage, [&] { RemoveMarker(EditingId); });
            ImGui::SameLine();
            if (ImGui::Button(UiLabel("Cancel##quickdelete").c_str())) ConfirmDelete = false;
        }
        if (!EditorMessage.empty()) ImGui::TextWrapped("%s", EditorMessage.c_str());
        if (!StorageError.empty()) ImGui::TextWrapped("%s", StorageError.c_str());
        KeepCurrentWindowOnScreen(&EditorWindowPos, &EditorWindowSize);
    }

    void DrawEditor(const State& state)
    {
        if (!PopupVisible) return;
        const bool worldEditor = EditingFromWorld;
        if (CancelEditor.exchange(false) || state.character != EditingCharacter || state.mapId != EditingMapId ||
            (!worldEditor && (!state.mapOpen || !Valid(state.fullMap))))
        {
            PopupVisible = false; FolderManagerVisible = false; SettingsVisible = false;
            return;
        }
        if (EditingExisting) { DrawQuickEditor(state); return; }
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const float scale = std::clamp(state.uiScale, .5f, 4.0f);
        AtlasTheme theme(scale);
        ImGui::SetNextWindowSize({360*scale, 0}, ImGuiCond_Appearing);
        ImGui::SetNextWindowSizeConstraints({std::min(360*scale,display.x-16),0}, {std::max(100.0f,display.x-16),std::max(100.0f,display.y-16)});
        ImGui::SetNextWindowPos({std::clamp(EditorAnchor.x, 8.0f, std::max(8.0f, display.x - 368*scale)),
                                 std::clamp(EditorAnchor.y, 8.0f, std::max(8.0f, display.y - 620*scale))}, ImGuiCond_Appearing);
        // Use a normal window so hidden overlays cannot retain modal input.
        const bool visible = ImGui::Begin("Vekk's Atlas###AtlasMarkerEditor", &PopupVisible, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
        EndWindow window;
        if (visible)
        {
            ImGui::TextColored({.86f,.71f,.40f,1}, Tr("YOUR PERSONAL ATLAS"));
            ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 120*scale);
            if (FolderButton(scale))
            {
                FolderManagerVisible = !FolderManagerVisible;
                if (FolderManagerVisible) { SettingsVisible = false; SelectManagerFolder(PendingFolderId); }
            }
            ImGui::SameLine();
            if (SettingsButton(scale)) { SettingsVisible = !SettingsVisible; if (SettingsVisible) FolderManagerVisible = false; }
        ImGui::SameLine(); if (RaidTools.Button(scale)) MarkDirty();
            ImGui::TextUnformatted(Tr("Save a location"));
            ImGui::TextDisabled(Tr("Choose an icon for this location"));
            ImGui::Separator();
            DrawGeneralIconChoices(false);
            if (IsWvWMap(state.mapType)) DrawWvWIconChoices(true);
            ImGui::TextUnformatted(Tr("Name"));
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##MarkerName", PendingName.data(), PendingName.size());
            ImGui::TextUnformatted(Tr("Folder"));
            ImGui::SetNextItemWidth(-1);
            FolderSelector("##MarkerFolder", state.character, PendingFolderId);
            DrawWorldHeightEditor(state);
            ImGui::Spacing();
            if (ImGui::Button(UiLabel("Cancel").c_str(), {125*scale,0})) { PopupVisible = false; FolderManagerVisible = false; SettingsVisible = false; }
            ImGui::SameLine();
            if (GoldButton(UiLabel("Save marker").c_str(), {-1,0}) && PopupVisible)
            {
                UIAction(EditorMessage, [&] { SavePendingMarker(state); });
            }
            ImGui::Separator();
            if (ImGui::Button(UiLabel("Paste marker").c_str(), {-1,0}))
            {
                PasteVisible = !PasteVisible;
                if (PasteVisible) UIAction(EditorMessage, [&] {
                    std::string text, error;
                    if (!ReadLinkFromClipboard(text, error)) throw std::runtime_error(error);
                    CopyText(PasteText, text);
                });
            }
            if (PasteVisible)
            {
                ImGui::TextWrapped(Tr("Paste a VA1:, VA2: or VA3: code to import the marker at its saved location."));
                ImGui::InputTextMultiline("##ShareCode", PasteText.data(), PasteText.size(), {-1,72*scale});
                if (GoldButton(UiLabel("Import marker").c_str(), {-1,0})) UIAction(EditorMessage, [&] { ImportMarkerLink(state); });
            }
            if (!EditorMessage.empty()) ImGui::TextWrapped("%s", EditorMessage.c_str());
            if (!StorageError.empty()) ImGui::TextWrapped("%s", StorageError.c_str());
            KeepCurrentWindowOnScreen(&EditorWindowPos, &EditorWindowSize);
        }
    }

    void DrawLanguageSelector()
    {
        const char* languages[] = {"English", "Español", "Deutsch"};
        int language = static_cast<int>(AtlasPreferences.language);
        if (ImGui::Combo(UiLabel("Language").c_str(), &language, languages, 3))
        {
            AtlasPreferences.language = static_cast<Language>(language);
            SetLanguage(AtlasPreferences.language);
            LoadMapNames();
            EditorMessage.clear();
            ManagerMessage.clear();
            InputStatus.store(Tr("No right-click received"));
            MarkDirty();
        }
    }

    void ProcessCapturedShortcut()
    {
        const uint32_t packet = CapturedShortcut.exchange(0, std::memory_order_acq_rel);
        if (!packet) return;
        const auto target = static_cast<ShortcutCapture>((packet >> 24) & 0xff);
        CapturingShortcut.store(ShortcutCapture::None, std::memory_order_release);
        if (packet & (1u << 20)) { ShortcutMessage = Tr("Shortcut capture canceled."); return; }
        Shortcut shortcut = packet & (1u << 19) ? Shortcut{} : UnpackShortcut(packet);
        if (MatchesShortcut({'N',false,true,true},shortcut.key,shortcut.alt,shortcut.ctrl,shortcut.shift))
        { ShortcutMessage=Tr("Ctrl+Shift+N is reserved for raid tools."); return; }
        Shortcut& destination = target == ShortcutCapture::Placement ? AtlasPreferences.placementShortcut : AtlasPreferences.settingsShortcut;
        const Shortcut& other = target == ShortcutCapture::Placement ? AtlasPreferences.settingsShortcut : AtlasPreferences.placementShortcut;
        if (shortcut.key && PackShortcut(shortcut) == PackShortcut(other))
        {
            ShortcutMessage = Tr("That shortcut is already assigned to another Atlas action.");
            return;
        }
        destination = shortcut;
        SyncShortcutState();
        ShortcutMessage = shortcut.key ? Tr("Shortcut updated.") : Tr("Shortcut cleared.");
        MarkDirty();
    }

    void DrawShortcutRow(const char* label, ShortcutCapture target, const Shortcut& shortcut)
    {
        ImGui::TextUnformatted(Tr(label));
        const auto capture = CapturingShortcut.load(std::memory_order_acquire);
        const std::string text = capture == target ? Tr("Press a key combination...") :
            shortcut.key ? ShortcutText(shortcut) : Tr("Not assigned");
        ImGui::Indent();
        ImGui::TextColored(capture == target ? ImVec4{.95f,.75f,.35f,1} : ImVec4{.82f,.84f,.82f,1}, "%s", text.c_str());
        ImGui::SameLine();
        ImGui::PushID(static_cast<int>(target));
        if (ImGui::SmallButton(Tr("Change")))
        {
            CapturingShortcut.store(target, std::memory_order_release);
            ShortcutMessage.clear();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton(Tr("Clear")))
        {
            Shortcut& destination = target == ShortcutCapture::Placement ? AtlasPreferences.placementShortcut : AtlasPreferences.settingsShortcut;
            destination = {};
            SyncShortcutState();
            CapturingShortcut.store(ShortcutCapture::None, std::memory_order_release);
            ShortcutMessage = Tr("Shortcut cleared.");
            MarkDirty();
        }
        ImGui::PopID();
        ImGui::Unindent();
        ImGui::Spacing();
    }

    bool DrawLauncherControls(float scale)
    {
        bool changed = false;
        const bool oldVisible = AtlasPreferences.launcherVisible;
        changed |= ImGui::Checkbox(UiLabel("Show Atlas launcher icon").c_str(), &AtlasPreferences.launcherVisible);
        if (oldVisible != AtlasPreferences.launcherVisible) SyncQuickAccess();
        if (QuickAccessAvailable())
            ImGui::TextWrapped(Tr("The icon now belongs to Nexus Quick Access. Move, align and resize it from Nexus settings."));
        else
        {
            ImGui::TextWrapped(Tr("Nexus Quick Access is unavailable; Atlas is using its fallback icon."));
            changed |= ImGui::SliderFloat(UiLabel("Launcher icon size").c_str(), &AtlasPreferences.launcherSize, 24.0f, 96.0f, "%.0f px");
            const ImVec2 display = ImGui::GetIO().DisplaySize;
            const float logicalWidth = display.x / std::max(scale, .5f);
            const float logicalHeight = display.y / std::max(scale, .5f);
            changed |= ImGui::SliderFloat(UiLabel("Horizontal position").c_str(), &AtlasPreferences.launcherX,
                0.0f, std::max(1.0f, logicalWidth - AtlasPreferences.launcherSize), "%.0f px");
            changed |= ImGui::SliderFloat(UiLabel("Vertical position").c_str(), &AtlasPreferences.launcherY,
                0.0f, std::max(1.0f, logicalHeight - AtlasPreferences.launcherSize), "%.0f px");
        }
        return changed;
    }

    bool DrawSettingsTabs(float scale)
    {
        bool changed = false;
        if (!ImGui::BeginTabBar("##AtlasSettingsTabs")) return false;
        if (ImGui::BeginTabItem(Tr("General")))
        {
            DrawLanguageSelector();
            const bool wasRaid=RaidTools.enabled;
            if (ImGui::Checkbox(UiLabel("Raid mode").c_str(), &RaidTools.enabled))
            { changed=true; RaidTools.windowVisible=RaidTools.enabled; if (wasRaid) { RaidTools.Finish(); RaidTools.eraser=false; } }
            if (RaidTools.enabled && ImGui::Button(UiLabel("Show raid tools").c_str())) RaidTools.windowVisible=true;
            ImGui::TextDisabled("Ctrl+Shift+N");
            changed |= ImGui::Checkbox(UiLabel("Show sharing help").c_str(), &AtlasPreferences.shareHelp);
            changed |= ImGui::Checkbox(UiLabel("Dark name background").c_str(), &AtlasPreferences.labelBackground);
            changed |= ImGui::Checkbox(UiLabel("Icon shadow and glow").c_str(), &AtlasPreferences.iconEffects);
            ImGui::TextDisabled(Tr("Settings are saved with your atlas and apply to all characters."));
            ImGui::Separator();
            ImGui::TextUnformatted(Tr("Share addon settings"));
            ImGui::TextWrapped(Tr("These settings already apply to every character on this installation. Copy the code to reuse them on another installation or share them with a friend."));
            if (ImGui::Button(UiLabel("Copy settings").c_str())) UIAction(SettingsShareMessage, [&] {
                std::string code, error;
                if (!EncodeSettingsLink(AtlasPreferences, code, error) || !CopyLinkToClipboard(code, error))
                    throw std::runtime_error(Tr(error.c_str()));
                CopyText(SettingsPasteText, code);
                SettingsShareMessage = Tr("Settings code copied.");
            });
            ImGui::InputTextMultiline("##SettingsShareCode", SettingsPasteText.data(), SettingsPasteText.size(), {-1, 54*scale});
            if (ImGui::Button(UiLabel("Paste from clipboard##settings").c_str())) UIAction(SettingsShareMessage, [&] {
                std::string code, error;
                if (!ReadLinkFromClipboard(code, error)) throw std::runtime_error(Tr(error.c_str()));
                CopyText(SettingsPasteText, code);
            });
            ImGui::SameLine();
            if (ImGui::Button(UiLabel("Apply settings").c_str())) UIAction(SettingsShareMessage, [&] {
                Preferences imported{};
                std::string error;
                if (!DecodeSettingsLink(SettingsPasteText.data(), imported, error)) throw std::runtime_error(Tr(error.c_str()));
                AtlasPreferences = imported;
                SetLanguage(imported.language);
                LoadMapNames();
                AdvancedModeEnabled.store(imported.advancedMode, std::memory_order_release);
                if (!imported.advancedMode) PendingAdvancedPlacement.store(false);
                SyncShortcutState();
                SyncQuickAccess();
                SettingsShareMessage = Tr("Settings applied.");
                MarkDirty();
            });
            if (!SettingsShareMessage.empty()) ImGui::TextWrapped("%s", SettingsShareMessage.c_str());
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(Tr("Markers")))
        {
            ImGui::TextWrapped(Tr("WvW markers are available in the 3D world only. Manage them through Folders."));
            if (ImGui::TreeNodeEx(UiLabel("Show marker names").c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth))
            {
                changed |= ImGui::Checkbox(UiLabel("On map##name_map").c_str(), &AtlasPreferences.nameMap);
                changed |= ImGui::Checkbox(UiLabel("On minimap##name_minimap").c_str(), &AtlasPreferences.nameMinimap);
                ImGui::TreePop();
            }
            if (ImGui::TreeNodeEx(UiLabel("Show marker distance").c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth))
            {
                changed |= ImGui::Checkbox(UiLabel("On map##distance_map").c_str(), &AtlasPreferences.distanceMap);
                changed |= ImGui::Checkbox(UiLabel("On minimap##distance_minimap").c_str(), &AtlasPreferences.distanceMinimap);
                ImGui::TreePop();
            }
            changed |= ImGui::SliderFloat(UiLabel("Map icon size").c_str(), &AtlasPreferences.mapIconSize, 20.0f, 72.0f, "%.0f px");
            changed |= ImGui::SliderFloat(UiLabel("Minimap icon size").c_str(), &AtlasPreferences.minimapIconSize, 16.0f, 56.0f, "%.0f px");
            changed |= ImGui::SliderFloat(UiLabel("Text size").c_str(), &AtlasPreferences.labelScale, .70f, 1.60f, "%.2fx");
            changed |= ImGui::SliderFloat(UiLabel("Icon opacity").c_str(), &AtlasPreferences.iconOpacity, .10f, 1.00f, "%.2f");
            changed |= ImGui::SliderFloat(UiLabel("Name opacity").c_str(), &AtlasPreferences.nameOpacity, .10f, 1.00f, "%.2f");
            changed |= ImGui::SliderFloat(UiLabel("Distance opacity").c_str(), &AtlasPreferences.distanceOpacity, .10f, 1.00f, "%.2f");
            changed |= ImGui::Checkbox(UiLabel("Minimap edge indicators").c_str(), &AtlasPreferences.edgeIndicators);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(Tr("Light Pillars")))
        {
            changed |= ImGui::Checkbox(UiLabel("World light beams").c_str(), &AtlasPreferences.worldBeacons);
            if (AtlasPreferences.worldBeacons)
            {
                float beaconPercent = AtlasPreferences.beaconOpacity * 100.0f;
                if (ImGui::SliderFloat(UiLabel("Beam intensity").c_str(), &beaconPercent, 10.0f, 100.0f, "%.0f%%"))
                { AtlasPreferences.beaconOpacity = beaconPercent / 100.0f; changed = true; }
                changed |= ImGui::SliderFloat(UiLabel("Beam width").c_str(), &AtlasPreferences.beaconWidth, .25f, 3.0f, "%.2fx");
                float distanceUnits = MetresToWorldUnits(AtlasPreferences.beaconMaxDistance);
                if (ImGui::SliderFloat(UiLabel("Beam distance").c_str(), &distanceUnits,
                    250.0f, MetresToWorldUnits(5000.0f), Tr("%.0f units")))
                { AtlasPreferences.beaconMaxDistance = WorldUnitsToMetres(std::clamp(distanceUnits, 250.0f, MetresToWorldUnits(5000.0f))); changed = true; }
                ImGui::TextWrapped(Tr("3D icons and labels shrink with camera depth, keeping a readable minimum size."));
                changed |= ImGui::Checkbox(UiLabel("Icon above the beam base").c_str(), &AtlasPreferences.worldIcons);
                changed |= ImGui::Checkbox(UiLabel("Show marker names in light pillars").c_str(), &AtlasPreferences.nameWorld);
                changed |= ImGui::Checkbox(UiLabel("Show marker distance in light pillars").c_str(), &AtlasPreferences.distanceWorld);
                changed |= ImGui::SliderFloat(UiLabel("World icon size").c_str(), &AtlasPreferences.worldIconScale, .50f, 2.00f, "%.2fx");
                changed |= ImGui::SliderFloat(UiLabel("World text size").c_str(), &AtlasPreferences.worldTextScale, .70f, 1.60f, "%.2fx");
                changed |= ImGui::SliderFloat(UiLabel("World icon opacity").c_str(), &AtlasPreferences.worldIconOpacity, .10f, 1.00f, "%.2f");
                changed |= ImGui::SliderFloat(UiLabel("World name opacity").c_str(), &AtlasPreferences.worldNameOpacity, .10f, 1.00f, "%.2f");
                changed |= ImGui::SliderFloat(UiLabel("World distance opacity").c_str(), &AtlasPreferences.worldDistanceOpacity, .10f, 1.00f, "%.2f");
            }
            const bool wasAdvanced = AtlasPreferences.advancedMode;
            changed |= ImGui::Checkbox(UiLabel("Enable advanced mode").c_str(), &AtlasPreferences.advancedMode);
            AdvancedModeEnabled.store(AtlasPreferences.advancedMode, std::memory_order_release);
            if (wasAdvanced && !AtlasPreferences.advancedMode) PendingAdvancedPlacement.store(false);
            ImGui::TextWrapped(Tr("Requires the GW2 RealTime API (RTAPI) addon. It lets you point at the 3D world and place a marker there with the Atlas shortcut."));
            ImGui::TextWrapped(Tr("In WvW, the same shortcut saves your character's position using MumbleLink. Advanced mode and RTAPI are not required."));
            if (AtlasPreferences.advancedMode)
            {
                const bool ready = RealTimeApiReady();
                ImGui::TextColored(ready ? ImVec4{.35f,.85f,.45f,1} : ImVec4{.95f,.48f,.30f,1}, "%s",
                    ready ? Tr("RTAPI detected and ready.") : Tr("RTAPI is not installed or is not currently loaded."));
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(Tr("Shortcuts")))
        {
            DrawShortcutRow("Place marker", ShortcutCapture::Placement, AtlasPreferences.placementShortcut);
            ImGui::TextWrapped(Tr("PvE: place at the 3D cursor. WvW: save your character's position."));
            DrawShortcutRow("Open Atlas settings", ShortcutCapture::Settings, AtlasPreferences.settingsShortcut);
            ImGui::TextDisabled(Tr("Press Escape to cancel capture. Backspace or Delete clears the shortcut."));
            if (!ShortcutMessage.empty()) ImGui::TextWrapped("%s", ShortcutMessage.c_str());
            ImGui::Separator();
            changed |= DrawLauncherControls(scale);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
        if (changed) MarkDirty();
        return changed;
    }

    void DrawSettings(const State& state)
    {
        if (!SettingsVisible) return;
        ProcessCapturedShortcut();
        const float scale = std::clamp(state.uiScale, .5f, 4.0f);
        AtlasTheme theme(scale);
        SettingsWindowSize = {440.0f * scale, 430.0f * scale};
        ImGui::SetNextWindowSize(SettingsWindowSize, ImGuiCond_Appearing);
        if (PopupVisible) ImGui::SetNextWindowPos(AttachedWindowPosition(SettingsWindowSize, scale), ImGuiCond_Always);
        else if (QuickAccessAvailable())
            ImGui::SetNextWindowPos({ImGui::GetIO().DisplaySize.x*.5f, ImGui::GetIO().DisplaySize.y*.5f},
                ImGuiCond_Appearing, {.5f,.5f});
        else
        {
            const float buttonSize = AtlasPreferences.launcherSize * scale;
            const ImVec2 position = ClampWindowPosition({AtlasPreferences.launcherX * scale + buttonSize + 8.0f * scale,
                AtlasPreferences.launcherY * scale}, SettingsWindowSize);
            ImGui::SetNextWindowPos(position, ImGuiCond_Appearing);
        }
        const bool visible = ImGui::Begin(UiLabel("Vekk's Atlas Settings###AtlasSettings").c_str(), &SettingsVisible,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
        EndWindow window;
        if (!visible) return;
        DrawSettingsTabs(scale);
        ImGui::Separator();
        if (ImGui::Button(UiLabel("Reset defaults").c_str()))
        {
            AtlasPreferences = Preferences{};
            RaidTools.Finish(); RaidTools.enabled=RaidTools.windowVisible=RaidTools.eraser=false;
            AdvancedModeEnabled.store(false, std::memory_order_release);
            SetLanguage(Language::English);
            SyncShortcutState();
            SyncQuickAccess();
            LoadMapNames();
            SettingsShareMessage.clear();
            MarkDirty();
        }
        KeepCurrentWindowOnScreen(nullptr, &SettingsWindowSize);
    }

    void DrawFolderManager(const State& state)
    {
        if (!FolderManagerVisible) return;
        if (!PopupVisible || state.character != EditingCharacter || state.mapId != EditingMapId ||
            (!EditingFromWorld && !state.mapOpen))
        { FolderManagerVisible = false; return; }
        const float scale = std::clamp(state.uiScale, .5f, 4.0f);
        const auto display = ImGui::GetIO().DisplaySize;
        AtlasTheme theme(scale);
        ImGui::SetNextWindowSize({std::min(760*scale,display.x-24),std::min(680*scale,display.y-24)}, ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(AttachedWindowPosition(FolderWindowSize, scale), ImGuiCond_Always);
        const bool visible = ImGui::Begin(UiLabel("Vekk's Atlas Folders###AtlasFolders").c_str(), &FolderManagerVisible, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
        EndWindow window;
        if (!visible) return;
        std::unordered_map<std::string, size_t> folderCounts;
        std::vector<const Marker*> listedMarkers;
        for (const auto& marker : Markers)
        {
            if (marker.character != state.character) continue;
            ++folderCounts[marker.folderId];
        }
        ImGui::TextColored({.86f,.71f,.40f,1}, Tr("LOCATION LIBRARY"));
        ImGui::TextWrapped(Tr("%s · Organize your maps without losing a location."), state.character.c_str());
        ImGui::Separator();
        ImGui::SetNextItemWidth(std::max(100.0f,ImGui::GetContentRegionAvail().x-130*scale));
        ImGui::InputText("##NewFolder", NewFolderName.data(), NewFolderName.size());
        ImGui::SameLine();
        if (GoldButton(UiLabel("Create folder").c_str(), {-1,0})) UIAction(ManagerMessage, [&] {
            const auto id = CreateFolder(state.character, NewFolderName.data());
            SelectManagerFolder(id); NewFolderName.fill(0);
            PendingFolderId = id;
        });
        ImGui::Spacing();
        ImGui::TextUnformatted(Tr("Paste marker folder"));
        ImGui::InputTextMultiline("##FolderShareCode", FolderPasteText.data(), FolderPasteText.size(), {-1,64*scale});
        if (ImGui::Button(UiLabel("Paste from clipboard").c_str())) UIAction(ManagerMessage, [&] { LoadFolderCodeFromClipboard(); });
        ImGui::SameLine();
        if (GoldButton(UiLabel("Import folder").c_str())) UIAction(ManagerMessage, [&] { ImportFolderLink(state); });
        if (AtlasPreferences.shareHelp)
            ImGui::TextDisabled(Tr("Import a folder and its markers, including saved 3D positions and elevation."));
        if (PendingFolderImport)
        {
            ImGui::TextWrapped(Tr("A folder named '%s' already exists. What would you like to do?"), PendingFolderImport->name.c_str());
            if (GoldButton(UiLabel("Replace existing folder").c_str())) UIAction(ManagerMessage, [&] {
                const auto imported = *PendingFolderImport;
                ApplyFolderImport(state, imported, PendingFolderImportTarget, false);
            });
            ImGui::SameLine();
            if (ImGui::Button(UiLabel("Create separate copy").c_str())) UIAction(ManagerMessage, [&] {
                const auto imported = *PendingFolderImport;
                ApplyFolderImport(state, imported, {}, true);
            });
            ImGui::SameLine();
            if (ImGui::Button(UiLabel("Cancel import").c_str()))
            { PendingFolderImport.reset(); PendingFolderImportTarget.clear(); ManagerMessage.clear(); }
        }
        ImGui::TextDisabled(Tr("Folders                                             Folder markers"));
        {
            ImGui::BeginChild("##FolderList", {190*scale,250*scale}, true);
            EndChild child;
            bool unfiledVisible = !UnfiledVisibility.contains(state.character) || UnfiledVisibility[state.character];
            if (EyeButton("UnfiledVisible", unfiledVisible, scale))
            { UnfiledVisibility[state.character] = !unfiledVisible; MarkDirty(); }
            ImGui::SameLine();
            if (ImGui::Selectable(UiLabel("Unfiled").c_str(), ManagerFolderId.empty())) SelectManagerFolder("");
            ImGui::TextDisabled(Tr("%zu locations"), folderCounts[""]);
            for (const auto& folder : Folders)
            {
                if (folder.character != state.character) continue;
                ScopedID id(folder.id.c_str());
                bool folderVisible = folder.visible;
                if (EyeButton("visible", folderVisible, scale))
                {
                    if (auto target = std::find_if(Folders.begin(), Folders.end(), [&](const Folder& item) { return item.id == folder.id; }); target != Folders.end())
                        target->visible = !folderVisible;
                    MarkDirty();
                }
                ImGui::SameLine();
                const auto position = ImGui::GetCursorScreenPos();
                if (ImGui::Selectable("##select", ManagerFolderId == folder.id)) SelectManagerFolder(folder.id);
                ImGui::GetWindowDrawList()->AddText(position, ImGui::GetColorU32(ImGuiCol_Text), folder.name.c_str());
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", folder.name.c_str());
                ImGui::TextDisabled(Tr("%zu locations"), folderCounts[folder.id]);
            }
        }
        ImGui::SameLine();
        {
            ImGui::BeginChild("##FolderMarkers", {0,250*scale}, true);
            EndChild child;
            for (const auto& marker : Markers)
                if (marker.character == state.character && marker.folderId == ManagerFolderId) listedMarkers.push_back(&marker);
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(listedMarkers.size()), 44*scale + ImGui::GetStyle().ItemSpacing.y);
            while (clipper.Step()) for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
            {
                const auto& marker = *listedMarkers[static_cast<size_t>(row)];
                ScopedID id(marker.id.c_str());
                const auto pos = ImGui::GetCursorScreenPos();
                if (ImGui::Selectable("##marker", SelectedManagerMarkerId == marker.id, 0, {0,44*scale}))
                { SelectedManagerMarkerId = marker.id; DeleteMarkerId.clear(); }
                auto* draw = ImGui::GetWindowDrawList();
                DrawIcon(draw, {pos.x+20*scale,pos.y+22*scale}, 18*scale, marker.icon);
                draw->AddText({pos.x+46*scale,pos.y+4*scale}, ImGui::GetColorU32(ImGuiCol_Text),
                    marker.name.empty() ? Tr(IconNames[static_cast<size_t>(marker.icon)]) : marker.name.c_str());
                char mapLabel[320]{};
                snprintf(mapLabel, sizeof(mapLabel), "%s%s", MapName(marker.mapId), marker.mapId == state.mapId ? Tr(" · current") : "");
                draw->AddText({pos.x+46*scale,pos.y+25*scale}, ImGui::GetColorU32(ImGuiCol_TextDisabled), mapLabel);
            }
            if (listedMarkers.empty()) ImGui::TextWrapped(Tr("This folder has no markers. Assign them using the editor's Folder selector."));
        }
        if (auto* marker = FindMarker(SelectedManagerMarkerId); marker && marker->character == state.character && marker->folderId == ManagerFolderId)
        {
            ImGui::TextUnformatted(marker->name.empty() ? Tr("Unnamed marker") : marker->name.c_str());
            if (ImGui::Button(UiLabel("Copy Marker").c_str())) UIAction(ManagerMessage, [&] { CopyMarkerLink(*marker, ManagerMessage); });
            ImGui::SameLine();
            if (ImGui::Button(UiLabel("Delete this marker").c_str())) DeleteMarkerId = marker->id;
            if (IsWvWMap(state.mapType) && marker->mapId == state.mapId)
            {
                ImGui::SameLine();
                if (ImGui::Button(UiLabel("Edit marker").c_str()))
                {
                    const std::string id = marker->id;
                    OpenEditor(state, marker->continent, id, std::nullopt, true);
                    return;
                }
            }
        }
        if (!DeleteMarkerId.empty())
        {
            ImGui::TextWrapped(Tr("Delete the selected marker? This cannot be undone in the addon."));
            if (ImGui::Button(UiLabel("Confirm marker deletion").c_str())) UIAction(ManagerMessage, [&] {
                RemoveMarker(DeleteMarkerId); DeleteMarkerId.clear(); SelectedManagerMarkerId.clear();
            });
            ImGui::SameLine();
            if (ImGui::Button(UiLabel("Cancel deletion##marker").c_str())) DeleteMarkerId.clear();
        }
        ImGui::Separator();
        if (!ManagerFolderId.empty())
        {
            ImGui::SetNextItemWidth(std::max(100.0f,ImGui::GetContentRegionAvail().x-390*scale));
            ImGui::InputText("##RenameFolder", RenameFolderName.data(), RenameFolderName.size());
            ImGui::SameLine();
            if (ImGui::Button(UiLabel("Rename").c_str())) UIAction(ManagerMessage, [&] {
                RenameFolder(state.character, ManagerFolderId, RenameFolderName.data()); ManagerMessage = Tr("Folder renamed.");
            });
            ImGui::SameLine();
            if (ImGui::Button(UiLabel("Copy folder").c_str())) UIAction(ManagerMessage, [&] { CopyFolderLink(ManagerFolderId, state.character, ManagerMessage); });
            ImGui::SameLine();
            if (ImGui::Button(UiLabel("Delete folder").c_str())) { DeleteFolderId = ManagerFolderId; DeleteTargetFolderId.clear(); DeleteMarkerId.clear(); }
            if (AtlasPreferences.shareHelp)
                ImGui::TextDisabled(Tr("Copy folder includes its name, markers, saved 3D positions and elevation."));
        }
        else ImGui::TextDisabled(Tr("Unfiled is the general category and cannot be deleted."));
        if (!DeleteFolderId.empty())
        {
            ImGui::Spacing();
            ImGui::TextColored({.95f,.66f,.40f,1}, Tr("BEFORE DELETING THE FOLDER"));
            ImGui::TextWrapped(Tr("Delete the %zu markers in '%s', or move them to another folder?"),
                FolderCount(state.character, DeleteFolderId), FolderName(DeleteFolderId, state.character));
            ImGui::SetNextItemWidth(-1);
            FolderSelector("##MoveDestination", state.character, DeleteTargetFolderId, DeleteFolderId);
            if (GoldButton(UiLabel("Move markers and delete folder").c_str())) UIAction(ManagerMessage, [&] { RemoveFolder(false); ManagerMessage = Tr("Folder deleted; markers preserved."); });
            ImGui::SameLine();
            if (ImGui::Button(UiLabel("Delete folder and markers").c_str())) UIAction(ManagerMessage, [&] { RemoveFolder(true); ManagerMessage = Tr("Folder and markers deleted."); });
            if (ImGui::Button(UiLabel("Cancel##folder").c_str())) DeleteFolderId.clear();
        }
        if (!ManagerMessage.empty()) ImGui::TextWrapped("%s", ManagerMessage.c_str());
        if (!StorageError.empty()) ImGui::TextWrapped(Tr("Save status: %s"), StorageError.c_str());
        KeepCurrentWindowOnScreen(nullptr, &FolderWindowSize);
    }

    void DrawLauncher(const State& state)
    {
        if (!AtlasPreferences.launcherVisible || QuickAccessAvailable()) return;
        const float scale = std::clamp(state.uiScale, .5f, 4.0f);
        const float side = AtlasPreferences.launcherSize * scale;
        const auto position = ClampLauncherPosition(AtlasPreferences.launcherX * scale, AtlasPreferences.launcherY * scale,
            side, ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y);
        ImGui::SetNextWindowPos({position.first, position.second}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({side, side}, ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0,0});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNav;
        if (ImGui::Begin("##VekksAtlasLauncher", nullptr, flags))
        {
            const ImVec2 start = ImGui::GetCursorScreenPos();
            if (ImGui::InvisibleButton("##OpenAtlasSettings", {side, side})) SettingsVisible = !SettingsVisible;
            auto* draw = ImGui::GetWindowDrawList();
            const bool hovered = ImGui::IsItemHovered();
            Texture_t* texture = LauncherTexture;
            if ((!texture || !texture->Resource) && IconTextures64[static_cast<size_t>(Icon::House)])
                texture = IconTextures64[static_cast<size_t>(Icon::House)];
            if (texture && texture->Resource)
            {
                const float inset = 1.0f * scale;
                draw->AddImage(texture->Resource, {start.x+inset,start.y+inset}, {start.x+side-inset,start.y+side-inset},
                    {0,0},{1,1},IM_COL32(255,255,255,205));
            }
            if (hovered) ImGui::SetTooltip("%s", Tr("Open Vekk's Atlas settings"));
        }
        ImGui::End();
        ImGui::PopStyleVar(2);
    }

    UINT InputCallback(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        static bool down = false;
        static POINT origin{};
        static ULONGLONG downAt = 0;
        if (!Ready.load()) { down = false; return message; }
        const bool keyDown = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
        const bool keyUp = message == WM_KEYUP || message == WM_SYSKEYUP;
        const auto capture = CapturingShortcut.load(std::memory_order_acquire);
        if (capture != ShortcutCapture::None && (keyDown || keyUp))
        {
            if (keyDown && (lParam & (1u << 30)) == 0)
            {
                const uint16_t key = static_cast<uint16_t>(wParam);
                uint32_t packet = static_cast<uint32_t>(capture) << 24;
                if (key == VK_ESCAPE) packet |= 1u << 20;
                else if (key == VK_BACK || key == VK_DELETE) packet |= 1u << 19;
                else if (!IsModifierKey(key))
                {
                    Shortcut shortcut{key, (GetKeyState(VK_MENU)&0x8000)!=0,
                        (GetKeyState(VK_CONTROL)&0x8000)!=0, (GetKeyState(VK_SHIFT)&0x8000)!=0};
                    packet |= PackShortcut(shortcut);
                }
                else return 0;
                CapturedShortcut.store(packet, std::memory_order_release);
            }
            return 0;
        }
        if (keyDown && (lParam & (1u << 30)) == 0 && HotkeysEnabled.load(std::memory_order_acquire))
        {
            const Shortcut pressed{static_cast<uint16_t>(wParam), (GetKeyState(VK_MENU)&0x8000)!=0,
                (GetKeyState(VK_CONTROL)&0x8000)!=0, (GetKeyState(VK_SHIFT)&0x8000)!=0};
            const uint32_t packed = PackShortcut(pressed);
            if (MatchesShortcut({'N',false,true,true},pressed.key,pressed.alt,pressed.ctrl,pressed.shift))
            { PendingRaidToggle.store(true,std::memory_order_release); return 0; }
            if (packed == ActiveSettingsShortcut.load(std::memory_order_acquire) && pressed.key)
            { PendingSettingsToggle.store(true, std::memory_order_release); return 0; }
            if (packed == ActivePlacementShortcut.load(std::memory_order_acquire) && pressed.key)
            { PendingAdvancedPlacement.store(true, std::memory_order_release); return 0; }
        }
        if (PopupOwnsInput.load() && wParam == VK_ESCAPE &&
            (keyDown || keyUp))
        {
            CancelEditor.store(true);
            return 0;
        }
        const POINT point{static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam))};
        if (message == WM_KILLFOCUS || message == WM_CANCELMODE)
        {
            down = false;
            PendingClick.store(0);
        }
        // Preserve a click already queued by WM_RBUTTONUP.
        if (message == WM_CAPTURECHANGED && lParam != 0 && reinterpret_cast<HWND>(lParam) != hwnd &&
            (GetKeyState(VK_RBUTTON) & 0x8000)) down = false;
        if (message == WM_RBUTTONDOWN || message == WM_RBUTTONDBLCLK)
        {
            const bool fresh = GetTickCount64() - AuthorFrameAt.load() < 500;
            const bool modifiers = (wParam & (MK_SHIFT | MK_CONTROL)) || (GetKeyState(VK_MENU) & 0x8000);
            down = fresh && CanAuthor.load() && !PopupOwnsInput.load() && !modifiers;
            InputStatus.store(!fresh ? Tr("Rendering suspended") : !CanAuthor.load() ? Tr("Creation unavailable when clicked") :
                PopupOwnsInput.load() ? Tr("Editor already open") : modifiers ? Tr("Click with modifier keys") : Tr("Button pressed"));
            origin = point;
            downAt = GetTickCount64();
        }
        if (message == WM_MOUSEMOVE && down && (std::abs(point.x - origin.x) > 5 || std::abs(point.y - origin.y) > 5))
        {
            down = false;
            InputStatus.store(Tr("Drag, not click"));
        }
        if (message == WM_RBUTTONUP)
        {
            if (down && CanAuthor.load() && !PopupOwnsInput.load() && GetTickCount64() - downAt < 700 &&
                std::abs(point.x - origin.x) <= 5 && std::abs(point.y - origin.y) <= 5)
            {
                PendingClick.store((uint64_t{1} << 32) | static_cast<uint32_t>(lParam));
                InputStatus.store(Tr("Click queued"));
            }
            else if (down) InputStatus.store(Tr("Click canceled: duration, movement or state change"));
            RightButtonUps.fetch_add(1);
            down = false;
        }
        return message;
    }

    void Render()
    {
        std::lock_guard lock(StateMutex);
        if (!Ready.load() || ImGui::GetCurrentContext() == nullptr) return;
        RenderedFrame.store(true);
        try
        {
        const State state = ReadState();
        const RaidContext raidContext{state.character,state.mapId,state.worldCamera.avatarPosition,
            state.avatarValid && !state.character.empty(),state.mapOpen,StorageWritable,state.uiScale};
        if (PendingRaidToggle.exchange(false)) { RaidTools.ToggleWindow(); MarkDirty(); }
        if (RaidTools.Update(raidContext)) MarkDirty();
        HotkeysEnabled.store(!state.character.empty() && !state.textFocused && !ImGui::GetIO().WantTextInput,
            std::memory_order_release);
        if (PendingSettingsToggle.exchange(false, std::memory_order_acq_rel)) SettingsVisible = !SettingsVisible;
        ProcessCapturedShortcut();
        RefreshTextures();
        ProcessAdvancedPlacement(state);
        std::string hoveredRoute;
        if (RaidTools.enabled && raidContext.valid && !state.mapOpen)
            hoveredRoute=DrawRaidPaths(ImGui::GetBackgroundDrawList(),state.worldCamera,RaidTools.paths,
                state.character,state.mapId,AtlasPreferences.beaconMaxDistance,state.uiScale,RaidTools.eraser);
        DrawWorldMarkerBeacons(state);
        if (RaidTools.enabled && RaidTools.eraser && raidContext.valid && !state.mapOpen && !PopupVisible &&
            !ImGui::GetIO().WantCaptureMouse && !ImGui::GetIO().WantTextInput && ImGui::IsMouseClicked(0))
            RaidTools.SelectForDeletion(HoveredRaidMarker.empty()?hoveredRoute:std::string{},HoveredRaidMarker);
        DrawMarkers(state, state.mapOpen ? state.fullMap : View{}, false);
        DrawMarkers(state, state.minimap, true);
        const bool mapAuthor = state.mapOpen && Valid(state.fullMap);
        const bool minimapAction = !state.mapOpen && Valid(state.minimap);
        const bool canAuthor = StorageWritable && (mapAuthor || minimapAction) && !state.character.empty() &&
            !state.textFocused && !PopupVisible && !ImGui::GetIO().WantCaptureMouse && !ImGui::GetIO().WantTextInput;
        AuthorStatus = !StorageWritable ? Tr("Saving blocked by file error") :
            (!mapAuthor && !minimapAction) || state.character.empty() ? state.reason :
            state.textFocused ? Tr("The game has an active text field") : PopupVisible ? Tr("Editor open") :
            ImGui::GetIO().WantCaptureMouse ? Tr("Another Nexus window captures the mouse") :
            ImGui::GetIO().WantTextInput ? Tr("Another Nexus window captures the keyboard") : Tr("Ready for right-click");
        CanAuthor.store(canAuthor);
        AuthorFrameAt.store(GetTickCount64());
        const uint64_t packet = PendingClick.exchange(0);
        if (packet && canAuthor)
        {
            const Point click{static_cast<float>(static_cast<int16_t>(packet & 0xffff)), static_cast<float>(static_cast<int16_t>((packet >> 16) & 0xffff))};
            if (mapAuthor)
            {
                if (const auto continent = ToContinent(state.fullMap, click))
                {
                    const auto existing = HitMarker(state, state.fullMap, click, false);
                    if (existing || !IsWvWMap(state.mapType))
                    {
                        OpenEditor(state, *continent, existing);
                        InputStatus.store(Tr("Editor open"));
                    }
                    else InputStatus.store(Tr("In WvW, use the placement shortcut to mark your character's position."));
                }
            }
            else if (minimapAction)
            {
                if (const auto marker = HitMarker(state, state.minimap, click, true))
                {
                    MinimapDeleteId = *marker;
                    MinimapDeleteAnchor = {click.x, click.y};
                    InputStatus.store(Tr("Minimap marker selected"));
                }
                else InputStatus.store(Tr("No marker under the cursor"));
            }
        }
        // Keep click diagnostics bounded and anonymous.
        static unsigned lastUps = 0;
        static unsigned reports = 0;
        const unsigned ups = RightButtonUps.load();
        if (ups != lastUps && reports < 20 && Api && Api->Log)
        {
            const std::string report = std::string("Right click: ") + InputStatus.load() + "; " + AuthorStatus;
            Api->Log(LOGL_INFO, AddonName, report.c_str());
            ++reports;
        }
        lastUps = ups;
        DrawEditor(state);
        DrawPendingMarker(state);
        DrawFolderManager(state);
        DrawLauncher(state);
        DrawSettings(state);
        if (RaidTools.Draw(raidContext,[&](Icon icon) {
            if (PopupVisible || !raidContext.valid || !StorageWritable) return;
            const Point point=IsWvWMap(state.mapType) ? CompetitivePlayerContinent(state.mapId,state.worldCamera.avatarPosition).value_or(Point{}) :
                Point{state.worldCamera.playerContinentX,state.worldCamera.playerContinentY};
            OpenEditor(state,point,std::nullopt,state.worldCamera.avatarPosition.y,true);
            PendingWorldPosition=state.worldCamera.avatarPosition; PendingIcon=icon;
        },[&](ImDrawList* draw,ImVec2 center,float radius,Icon icon) { DrawIcon(draw,center,radius,icon); },
          [&](const std::string& id) { RemoveMarker(id, state.character); })) MarkDirty();
        DrawMinimapDelete(state);
        PopupOwnsInput.store(PopupVisible);
        if (PopupVisible) CanAuthor.store(false);
        if (Dirty && std::chrono::steady_clock::now() - DirtySince > std::chrono::seconds(1)) SaveMarkers();
        }
        catch (const std::exception& error)
        {
            CanAuthor.store(false);
            Ready.store(false);
            if (Api && Api->Log) Api->Log(LOGL_WARNING, AddonName, error.what());
        }
    }

    void AfterFrame()
    {
        // Expire input state when the main render callback was skipped.
        if (!RenderedFrame.exchange(false))
        {
            CanAuthor.store(false);
            PopupOwnsInput.store(false);
            PendingClick.store(0);
        }
    }

    void Options()
    {
        std::lock_guard lock(StateMutex);
        ImGui::TextUnformatted("Vekk's Atlas 1.1.0");
        ImGui::TextUnformatted("Leir.6218");
        ImGui::TextWrapped("AI Notice: developed by Leir.6218 with AI assistance.");
        if (!Ready.load())
        {
            ImGui::TextWrapped(Tr("Addon disabled after an error. Check addons/Nexus/Nexus.log, then reload the addon."));
            return;
        }
        ProcessCapturedShortcut();
        const float scale = std::clamp(NexusLink ? NexusLink->Scaling : 1.0f, .5f, 4.0f);
        DrawSettingsTabs(scale);
        ImGui::Separator();
        ImGui::Text(Tr("Saved markers: %zu"), Markers.size());
        ImGui::SameLine();
        ImGui::Text(Tr("Folders: %zu"), Folders.size());
        if (!StorageError.empty()) ImGui::TextWrapped(Tr("Save status: %s"), StorageError.c_str());
        else ImGui::TextUnformatted(Dirty ? Tr("Unsaved changes...") : Tr("All changes saved."));
    }

    std::filesystem::path AddonDirectory()
    {
        if (Api && Api->Paths_GetAddonDirectory)
        {
            if (const char* path = Api->Paths_GetAddonDirectory(StorageFolder)) return std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t*>(path)));
        }
        throw std::runtime_error(Tr("Nexus did not provide the addon data folder."));
    }

    void Unload();
    void Load(AddonAPI_t* api)
    {
        Api = api;
        if (!Api || !Api->ImguiContext || !Api->ImguiMalloc || !Api->ImguiFree || !Api->DataLink_Get || !Api->GUI_Register || !Api->GUI_Deregister) return;
        try
        {
        ImGui::SetAllocatorFunctions(reinterpret_cast<void* (*)(size_t, void*)>(Api->ImguiMalloc), reinterpret_cast<void (*)(void*, void*)>(Api->ImguiFree));
        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(Api->ImguiContext));
        MumbleLink = static_cast<Mumble::Data*>(Api->DataLink_Get("DL_MUMBLE_LINK"));
        NexusLink = static_cast<NexusLinkData_t*>(Api->DataLink_Get("DL_NEXUS_LINK"));
        if (!MumbleLink || !NexusLink) throw std::runtime_error(Tr("Nexus did not publish MumbleLink/NexusLink; rendering is not registered."));
        if (!Api->Textures_GetOrCreateFromResource || !Api->Textures_Get || !Module)
            throw std::runtime_error(Tr("Nexus does not provide the icon service."));
        for (size_t i = 0; i < IconTextures128.size(); ++i)
        {
            IconTextures128[i] = Api->Textures_GetOrCreateFromResource(TextureIds128[i], static_cast<uint32_t>(201 + i), Module);
            IconTextures64[i] = Api->Textures_GetOrCreateFromResource(TextureIds64[i], static_cast<uint32_t>(301 + i), Module);
        }
        LauncherTexture = Api->Textures_GetOrCreateFromResource(LauncherTextureId, 401, Module);
        DataFile = AddonDirectory() / "markers.json";
        bool migrated = false;
        const auto newPath = DataFile;
        if (!std::filesystem::exists(newPath))
        {
            if (const char* path = Api->Paths_GetAddonDirectory("GW2 Custom Map Markers"))
            {
                const auto legacy = std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t*>(path))) / "CustomMapMarkers" / "markers.json";
                if (std::filesystem::exists(legacy)) { DataFile = legacy; migrated = true; }
            }
        }
        LoadMarkers();
        AdvancedModeEnabled.store(AtlasPreferences.advancedMode, std::memory_order_release);
        SyncShortcutState();
        SyncQuickAccess();
        LoadMapNames();
        DataFile = newPath;
        if (migrated && StorageWritable) MarkDirty();
        Ready.store(true);
        RealTimeApi.store(static_cast<RTAPI::RealTimeData*>(Api->DataLink_Get(DL_RTAPI)), std::memory_order_release);
        if (Api->Events_Subscribe && Api->Events_Unsubscribe)
        {
            Api->Events_Subscribe(EV_ADDON_LOADED, OnAddonLoaded);
            Api->Events_Subscribe(EV_ADDON_UNLOADED, OnAddonUnloaded);
            AddonEventsRegistered = true;
        }
        Api->GUI_Register(RT_PostRender, AfterFrame); FrameRegistered = true;
        Api->GUI_Register(RT_Render, Render); RenderRegistered = true;
        Api->GUI_Register(RT_OptionsRender, Options); OptionsRegistered = true;
        if (Api->WndProc_Register) { Api->WndProc_Register(InputCallback); InputRegistered = true; }
        if (Api->Log) Api->Log(LOGL_INFO, AddonName, "Vekk's Atlas 1.1.0 loaded; WvW markers use saved MumbleLink world positions; WvW map overlays are disabled.");
        }
        catch (const std::exception& error)
        {
            if (Api && Api->Log) Api->Log(LOGL_WARNING, AddonName, error.what());
            Unload();
        }
    }

    void Unload()
    {
        Ready.store(false);
        CanAuthor.store(false);
        PopupOwnsInput.store(false);
        HotkeysEnabled.store(false, std::memory_order_release);
        PendingAdvancedPlacement.store(false);
        PendingSettingsToggle.store(false);
        PendingRaidToggle.store(false);
        AdvancedModeEnabled.store(false, std::memory_order_release);
        CapturingShortcut.store(ShortcutCapture::None, std::memory_order_release);
        CapturedShortcut.store(0, std::memory_order_release);
        ActivePlacementShortcut.store(0, std::memory_order_release);
        ActiveSettingsShortcut.store(0, std::memory_order_release);
        RealTimeApi.store(nullptr, std::memory_order_release);
        if (Api && QuickAccessRegistered && Api->QuickAccess_Remove) Api->QuickAccess_Remove(QuickAccessId);
        QuickAccessRegistered = false;
        if (Api && QuickAccessBindRegistered && Api->InputBinds_Deregister) Api->InputBinds_Deregister(QuickAccessBindId);
        QuickAccessBindRegistered = false;
        if (Api && AddonEventsRegistered && Api->Events_Unsubscribe)
        {
            Api->Events_Unsubscribe(EV_ADDON_LOADED, OnAddonLoaded);
            Api->Events_Unsubscribe(EV_ADDON_UNLOADED, OnAddonUnloaded);
        }
        if (Api && InputRegistered && Api->WndProc_Deregister) Api->WndProc_Deregister(InputCallback);
        if (Api && RenderRegistered && Api->GUI_Deregister) Api->GUI_Deregister(Render);
        if (Api && FrameRegistered && Api->GUI_Deregister) Api->GUI_Deregister(AfterFrame);
        if (Api && OptionsRegistered && Api->GUI_Deregister) Api->GUI_Deregister(Options);
        std::lock_guard lock(StateMutex);
        if (RaidTools.Finish()) MarkDirty();
        SaveMarkers();
        InputRegistered = RenderRegistered = OptionsRegistered = FrameRegistered = false;
        AddonEventsRegistered = false;
        PendingClick.store(0);
        RightButtonUps.store(0);
        InputStatus.store(Tr("No right-click received"));
        AuthorStatus = Tr("Waiting for the map");
        RenderedFrame.store(false);
        CancelEditor.store(false);
        PopupVisible = FolderManagerVisible = SettingsVisible = false;
        EditingExisting = EditingFromWorld = ConfirmDelete = PasteVisible = false;
        PendingWorldPosition.reset();
        PendingFolderImport.reset();
        EditingId.clear(); EditingCharacter.clear(); PendingFolderId.clear(); ManagerFolderId.clear();
        DeleteFolderId.clear(); DeleteTargetFolderId.clear(); DeleteMarkerId.clear(); SelectedManagerMarkerId.clear();
        PendingFolderImportTarget.clear(); MinimapDeleteId.clear(); EditorMessage.clear(); ManagerMessage.clear();
        AdvancedStatus.clear(); ShortcutMessage.clear(); SettingsShareMessage.clear(); WorldHeightReferences.clear();
        BeaconReference.Reset();
        PendingName.fill(0); NewFolderName.fill(0); RenameFolderName.fill(0); PasteText.fill(0); SettingsPasteText.fill(0);
        std::fill(FolderPasteText.begin(), FolderPasteText.end(), char{0});
        WorldBeaconCandidates = WorldBeaconsDrawn = 0;
        Markers.clear(); Folders.clear(); UnfiledVisibility.clear(); RaidTools.Reset(); HoveredRaidMarker.clear();
        MapNames.clear();
        IconTextures128.fill(nullptr); IconTextures64.fill(nullptr); LauncherTexture = nullptr;
        DataFile.clear(); SchemaOneSource.clear(); StorageError.clear(); Dirty = false;
        StorageWritable = true;
        MumbleLink = nullptr; NexusLink = nullptr; Api = nullptr;
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) cmm::Module = module;
    return TRUE;
}

extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef()
{
    static AddonDefinition_t definition{};
    definition.Signature = static_cast<uint32_t>(-42424121);
    definition.APIVersion = NEXUS_API_VERSION;
    definition.Name = cmm::AddonName;
    definition.Version = {1, 1, 0, 0};
    definition.Author = "Leir.6218";
    definition.Description = "Personal map, minimap, WvW and 3D light markers with optional RTAPI cursor placement.";
    definition.Load = cmm::Load;
    definition.Unload = cmm::Unload;
    definition.Flags = AF_None;
    definition.Provider = UP_None;
    definition.UpdateLink = nullptr;
    return &definition;
}
