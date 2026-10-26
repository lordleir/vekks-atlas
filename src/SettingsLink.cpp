#include "SettingsLink.h"
#include "WorldBeacons.h"

#include <Windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace cmm
{
    namespace
    {
        constexpr std::string_view Prefix = "VAS2:";
        constexpr std::string_view LegacyPrefix = "VAS1:";
        constexpr size_t DigestBytes = 8;

        template <typename T> void Append(std::vector<uint8_t>& out, const T& value)
        {
            const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
            out.insert(out.end(), bytes, bytes + sizeof(T));
        }

        template <typename T> bool Read(const std::vector<uint8_t>& in, size_t& offset, T& value)
        {
            if (offset + sizeof(T) > in.size()) return false;
            std::memcpy(&value, in.data() + offset, sizeof(T));
            offset += sizeof(T);
            return true;
        }

        bool Hash(const uint8_t* data, size_t size, std::array<uint8_t, 32>& digest)
        {
            BCRYPT_ALG_HANDLE algorithm{};
            BCRYPT_HASH_HANDLE hash{};
            DWORD objectSize{}, written{};
            std::vector<uint8_t> object;
            bool ok = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0;
            if (ok) ok = BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize), &written, 0) == 0;
            if (ok) object.resize(objectSize);
            if (ok) ok = BCryptCreateHash(algorithm, &hash, object.data(), objectSize, nullptr, 0, 0) == 0;
            if (ok) ok = BCryptHashData(hash, const_cast<PUCHAR>(data), static_cast<ULONG>(size), 0) == 0;
            if (ok) ok = BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) == 0;
            if (hash) BCryptDestroyHash(hash);
            if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
            return ok;
        }

        std::string Base64Url(const std::vector<uint8_t>& bytes)
        {
            static constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
            std::string result;
            result.reserve((bytes.size() * 4 + 2) / 3);
            uint32_t buffer{};
            int bits{};
            for (uint8_t byte : bytes)
            {
                buffer = (buffer << 8) | byte;
                bits += 8;
                while (bits >= 6) { bits -= 6; result.push_back(alphabet[(buffer >> bits) & 63]); }
            }
            if (bits) result.push_back(alphabet[(buffer << (6 - bits)) & 63]);
            return result;
        }

        int DecodeChar(char value)
        {
            if (value >= 'A' && value <= 'Z') return value - 'A';
            if (value >= 'a' && value <= 'z') return value - 'a' + 26;
            if (value >= '0' && value <= '9') return value - '0' + 52;
            if (value == '-') return 62;
            if (value == '_') return 63;
            return -1;
        }

        bool DecodeBase64Url(std::string_view text, std::vector<uint8_t>& bytes)
        {
            bytes.clear();
            uint32_t buffer{};
            int bits{};
            for (char character : text)
            {
                const int value = DecodeChar(character);
                if (value < 0) return false;
                buffer = (buffer << 6) | static_cast<uint32_t>(value);
                bits += 6;
                if (bits >= 8) { bits -= 8; bytes.push_back(static_cast<uint8_t>((buffer >> bits) & 255)); }
            }
            return bits < 6;
        }

        bool Valid(const Preferences& p)
        {
            const auto range = [](float value, float lo, float hi)
                { return std::isfinite(value) && value >= lo && value <= hi; };
            return p.language >= Language::English && p.language <= Language::German &&
                range(p.mapIconSize, 20, 72) && range(p.minimapIconSize, 16, 56) &&
                range(p.labelScale, .70f, 1.60f) && range(p.iconOpacity, .10f, 1) &&
                range(p.nameOpacity, .10f, 1) && range(p.distanceOpacity, .10f, 1) &&
                range(p.beaconOpacity, .10f, 1) && range(p.beaconMaxDistance, WorldUnitsToMetres(250), 5000) &&
                range(p.beaconWidth, .25f, 3.0f) &&
                range(p.worldIconScale, .50f, 2) && range(p.worldTextScale, .70f, 1.60f) &&
                range(p.worldIconOpacity, .10f, 1) && range(p.worldNameOpacity, .10f, 1) &&
                range(p.worldDistanceOpacity, .10f, 1) && range(p.launcherSize, 24, 96) &&
                range(p.launcherX, -10000, 10000) && range(p.launcherY, -10000, 10000) &&
                !IsModifierKey(p.placementShortcut.key) && !IsModifierKey(p.settingsShortcut.key);
        }

        void AppendShortcut(std::vector<uint8_t>& data, const Shortcut& shortcut)
        {
            Append(data, shortcut.key);
            const uint8_t modifiers = (shortcut.alt ? 1 : 0) | (shortcut.ctrl ? 2 : 0) | (shortcut.shift ? 4 : 0);
            Append(data, modifiers);
        }

        bool ReadShortcut(const std::vector<uint8_t>& data, size_t& offset, Shortcut& shortcut)
        {
            uint8_t modifiers{};
            if (!Read(data, offset, shortcut.key) || !Read(data, offset, modifiers) || (modifiers & ~7)) return false;
            shortcut.alt = (modifiers & 1) != 0;
            shortcut.ctrl = (modifiers & 2) != 0;
            shortcut.shift = (modifiers & 4) != 0;
            return true;
        }
    }

    bool EncodeSettingsLink(const Preferences& p, std::string& code, std::string& error)
    {
        code.clear(); error.clear();
        if (!Valid(p)) { error = "Invalid settings."; return false; }
        std::vector<uint8_t> data;
        data.reserve(96);
        Append(data, static_cast<uint8_t>(p.language));
        uint32_t flags{};
        const bool values[] = {p.nameMap,p.nameMinimap,p.nameWorld,p.distanceMap,p.distanceMinimap,p.distanceWorld,
            p.edgeIndicators,p.iconEffects,p.labelBackground,p.shareHelp,p.worldBeacons,p.worldIcons,
            p.advancedMode,p.launcherVisible};
        for (size_t i=0;i<std::size(values);++i) if (values[i]) flags |= 1u << i;
        Append(data, flags);
        for (float value : {p.mapIconSize,p.minimapIconSize,p.labelScale,p.iconOpacity,p.nameOpacity,
            p.distanceOpacity,p.beaconOpacity,p.beaconMaxDistance,p.worldIconScale,p.worldTextScale,
            p.worldIconOpacity,p.worldNameOpacity,p.worldDistanceOpacity,p.launcherSize,p.launcherX,p.launcherY}) Append(data, value);
        AppendShortcut(data, p.placementShortcut);
        AppendShortcut(data, p.settingsShortcut);
        Append(data, p.beaconWidth);
        std::array<uint8_t,32> digest{};
        if (!Hash(data.data(), data.size(), digest)) { error = "Could not initialize SHA-256."; return false; }
        data.insert(data.end(), digest.begin(), digest.begin() + DigestBytes);
        code = std::string(Prefix) + Base64Url(data);
        if (code.size() > 256) { code.clear(); error = "Settings code exceeds 256 characters."; return false; }
        return true;
    }

    bool DecodeSettingsLink(std::string_view code, Preferences& settings, std::string& error)
    {
        error.clear();
        const bool version2 = code.starts_with(Prefix);
        if ((!version2 && !code.starts_with(LegacyPrefix)) || code.size() > 256) { error = "Invalid settings code."; return false; }
        std::vector<uint8_t> data;
        if (!DecodeBase64Url(code.substr(Prefix.size()), data) || data.size() <= DigestBytes)
        { error = "Invalid settings code."; return false; }
        if (Base64Url(data) != code.substr(Prefix.size()))
        { error = "Invalid settings code."; return false; }
        const size_t payloadSize = data.size() - DigestBytes;
        std::array<uint8_t,32> digest{};
        if (!Hash(data.data(), payloadSize, digest) ||
            !std::equal(digest.begin(), digest.begin()+DigestBytes, data.begin()+payloadSize))
        { error = "The settings code is damaged or modified."; return false; }
        Preferences decoded{};
        size_t offset{};
        uint8_t language{};
        uint32_t flags{};
        if (!Read(data,offset,language) || !Read(data,offset,flags) || language > 2 || (flags & ~0x3fffu))
        { error = "Invalid settings data."; return false; }
        decoded.language = static_cast<Language>(language);
        bool* values[] = {&decoded.nameMap,&decoded.nameMinimap,&decoded.nameWorld,&decoded.distanceMap,
            &decoded.distanceMinimap,&decoded.distanceWorld,&decoded.edgeIndicators,&decoded.iconEffects,
            &decoded.labelBackground,&decoded.shareHelp,&decoded.worldBeacons,&decoded.worldIcons,
            &decoded.advancedMode,&decoded.launcherVisible};
        for (size_t i=0;i<std::size(values);++i) *values[i] = (flags & (1u << i)) != 0;
        float* numbers[] = {&decoded.mapIconSize,&decoded.minimapIconSize,&decoded.labelScale,&decoded.iconOpacity,
            &decoded.nameOpacity,&decoded.distanceOpacity,&decoded.beaconOpacity,&decoded.beaconMaxDistance,
            &decoded.worldIconScale,&decoded.worldTextScale,&decoded.worldIconOpacity,&decoded.worldNameOpacity,
            &decoded.worldDistanceOpacity,&decoded.launcherSize,&decoded.launcherX,&decoded.launcherY};
        for (float* number : numbers) if (!Read(data,offset,*number)) { error = "Incomplete settings data."; return false; }
        if (!ReadShortcut(data,offset,decoded.placementShortcut) || !ReadShortcut(data,offset,decoded.settingsShortcut) ||
            (version2 && !Read(data,offset,decoded.beaconWidth)) ||
            offset != payloadSize || !Valid(decoded)) { error = "Invalid settings data."; return false; }
        settings = decoded;
        return true;
    }
}
