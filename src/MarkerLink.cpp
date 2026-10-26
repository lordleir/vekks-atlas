#include "AtlasLocalization.h"
#include "MarkerLink.h"

#include <Windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace cmm
{
    namespace
    {
        constexpr std::string_view MarkerPrefixV1 = "VA1:";
        constexpr std::string_view MarkerPrefixV2 = "VA2:";
        constexpr std::string_view MarkerPrefixV3 = "VA3:";
        constexpr std::string_view FolderPrefixV1 = "VAF1:";
        constexpr std::string_view FolderPrefixV2 = "VAF2:";
        constexpr std::string_view FolderPrefixV3 = "VAF3:";
        constexpr std::string_view Alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
        constexpr size_t ChecksumSize = 8;
        constexpr size_t MarkerHeaderV1 = 23;
        constexpr size_t MarkerHeaderV2 = 24;
        constexpr size_t FolderHeaderSize = 6;
        constexpr size_t FolderMarkerHeaderV1 = 23;
        constexpr size_t FolderMarkerHeaderV2 = 24;
        constexpr unsigned char HasWorldHeight = 1;
        constexpr unsigned char HasWorldPosition = 2;
        constexpr size_t MaxFolderCodeSize = 1024 * 1024;
        constexpr size_t MaxFolderPayloadSize = ((MaxFolderCodeSize - FolderPrefixV2.size()) / 4) * 3;
        using Bytes = std::vector<unsigned char>;

        std::array<unsigned char, 32> Digest(const Bytes& bytes)
        {
            BCRYPT_ALG_HANDLE algorithm{};
            if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
                throw std::runtime_error(Tr("Could not initialize SHA-256."));
            std::array<unsigned char, 32> result{};
            const auto status = BCryptHash(algorithm, nullptr, 0, const_cast<PUCHAR>(bytes.data()),
                static_cast<ULONG>(bytes.size()), result.data(), static_cast<ULONG>(result.size()));
            BCryptCloseAlgorithmProvider(algorithm, 0);
            if (status < 0) throw std::runtime_error(Tr("Could not verify the code's integrity."));
            return result;
        }

        void Append32(Bytes& bytes, uint32_t value)
        {
            for (unsigned shift = 0; shift < 32; shift += 8) bytes.push_back(static_cast<unsigned char>(value >> shift));
        }

        void Append16(Bytes& bytes, uint16_t value)
        {
            bytes.push_back(static_cast<unsigned char>(value));
            bytes.push_back(static_cast<unsigned char>(value >> 8));
        }

        uint16_t Read16(const Bytes& bytes, size_t offset)
        {
            return static_cast<uint16_t>(bytes[offset] | (uint16_t{bytes[offset + 1]} << 8));
        }

        uint32_t Read32(const Bytes& bytes, size_t offset)
        {
            uint32_t value = 0;
            for (unsigned i = 0; i < 4; ++i) value |= uint32_t{bytes[offset + i]} << (i * 8);
            return value;
        }

        bool ValidHeight(const std::optional<float>& height)
        {
            return !height || (std::isfinite(*height) && std::abs(*height) <= 100000.0f);
        }

        bool ValidPayload(const ShareableMarker& marker, size_t nameLimit, size_t folderLimit)
        {
            return marker.mapId > 0 && static_cast<unsigned>(marker.icon) <= static_cast<unsigned>(Icon::ShieldGenerator) &&
                std::isfinite(marker.continent.x) && std::isfinite(marker.continent.y) &&
                std::abs(marker.continent.x) <= 10000000.0f && std::abs(marker.continent.y) <= 10000000.0f &&
                ValidHeight(marker.worldHeight) && (!marker.worldPosition ||
                    (ValidHeight(marker.worldPosition->x) && ValidHeight(marker.worldPosition->y) && ValidHeight(marker.worldPosition->z))) &&
                ValidLabel(marker.name, nameLimit) && ValidLabel(marker.folderName, folderLimit);
        }

        void AppendEncoded(std::string_view prefix, const Bytes& bytes, std::string& encoded)
        {
            encoded.assign(prefix);
            uint32_t buffer = 0;
            unsigned bits = 0;
            for (auto byte : bytes)
            {
                buffer = (buffer << 8) | byte;
                bits += 8;
                while (bits >= 6)
                {
                    bits -= 6;
                    encoded += Alphabet[(buffer >> bits) & 63];
                }
            }
            if (bits) encoded += Alphabet[(buffer << (6 - bits)) & 63];
        }

        Bytes DecodeEncoded(std::string_view encoded)
        {
            if (encoded.empty() || encoded.size() % 4 == 1) throw std::runtime_error(Tr("Incomplete code."));
            Bytes bytes;
            bytes.reserve(encoded.size() * 3 / 4 + 1);
            uint32_t buffer = 0;
            unsigned bits = 0;
            for (char c : encoded)
            {
                const auto value = Alphabet.find(c);
                if (value == std::string_view::npos) throw std::runtime_error(Tr("The code contains invalid characters."));
                buffer = (buffer << 6) | static_cast<uint32_t>(value);
                bits += 6;
                if (bits >= 8)
                {
                    bits -= 8;
                    bytes.push_back(static_cast<unsigned char>(buffer >> bits));
                }
            }
            if (bits && (buffer & ((1u << bits) - 1))) throw std::runtime_error(Tr("Noncanonical encoding."));
            return bytes;
        }

        void VerifyAndRemoveChecksum(Bytes& bytes)
        {
            if (bytes.size() < ChecksumSize) throw std::runtime_error(Tr("Incomplete code."));
            std::array<unsigned char, ChecksumSize> stored{};
            std::copy(bytes.end() - ChecksumSize, bytes.end(), stored.begin());
            bytes.resize(bytes.size() - ChecksumSize);
            const auto digest = Digest(bytes);
            unsigned difference = 0;
            for (size_t i = 0; i < ChecksumSize; ++i) difference |= stored[i] ^ digest[i];
            if (difference) throw std::runtime_error(Tr("The code is damaged or modified (integrity check failed)."));
        }

        void Trim(std::string_view& text)
        {
            while (!text.empty() && (text.front() == ' ' || text.front() == '\r' || text.front() == '\n' || text.front() == '\t')) text.remove_prefix(1);
            while (!text.empty() && (text.back() == ' ' || text.back() == '\r' || text.back() == '\n' || text.back() == '\t')) text.remove_suffix(1);
        }

        void AppendMarkerRecord(Bytes& bytes, const ShareableMarker& marker, bool folder)
        {
            Append32(bytes, marker.mapId);
            Append32(bytes, marker.continentId);
            Append32(bytes, std::bit_cast<uint32_t>(marker.floorId));
            Append32(bytes, std::bit_cast<uint32_t>(marker.continent.x));
            Append32(bytes, std::bit_cast<uint32_t>(marker.continent.y));
            bytes.push_back(static_cast<unsigned char>(marker.icon));
            bytes.push_back((marker.worldHeight ? HasWorldHeight : 0) | (marker.worldPosition ? HasWorldPosition : 0));
            if (folder) Append16(bytes, static_cast<uint16_t>(marker.name.size()));
            else
            {
                bytes.push_back(static_cast<unsigned char>(marker.name.size()));
                bytes.push_back(static_cast<unsigned char>(marker.folderName.size()));
            }
            if (marker.worldHeight) Append32(bytes, std::bit_cast<uint32_t>(*marker.worldHeight));
            if (marker.worldPosition)
            {
                Append32(bytes, std::bit_cast<uint32_t>(marker.worldPosition->x));
                Append32(bytes, std::bit_cast<uint32_t>(marker.worldPosition->y));
                Append32(bytes, std::bit_cast<uint32_t>(marker.worldPosition->z));
            }
            bytes.insert(bytes.end(), marker.name.begin(), marker.name.end());
            if (!folder) bytes.insert(bytes.end(), marker.folderName.begin(), marker.folderName.end());
        }

        ShareableMarker DecodeMarkerRecord(const Bytes& bytes, size_t& offset, bool version2, bool folder, bool version3)
        {
            const size_t header = folder ? (version2 ? FolderMarkerHeaderV2 : FolderMarkerHeaderV1) :
                (version2 ? MarkerHeaderV2 : MarkerHeaderV1);
            if (offset + header > bytes.size()) throw std::runtime_error(Tr("Incomplete folder marker."));

            ShareableMarker marker;
            marker.mapId = Read32(bytes, offset);
            marker.continentId = Read32(bytes, offset + 4);
            marker.floorId = std::bit_cast<int32_t>(Read32(bytes, offset + 8));
            marker.continent = {std::bit_cast<float>(Read32(bytes, offset + 12)), std::bit_cast<float>(Read32(bytes, offset + 16))};
            marker.icon = static_cast<Icon>(bytes[offset + 20]);

            unsigned char flags = 0;
            size_t nameLength = 0;
            size_t folderLength = 0;
            if (version2)
            {
                flags = bytes[offset + 21];
                if (flags & ~(HasWorldHeight | (version3 ? HasWorldPosition : 0)))
                    throw std::runtime_error(Tr("Unsupported marker flags."));
                if (folder) nameLength = Read16(bytes, offset + 22);
                else
                {
                    nameLength = bytes[offset + 22];
                    folderLength = bytes[offset + 23];
                }
            }
            else if (folder) nameLength = Read16(bytes, offset + 21);
            else
            {
                nameLength = bytes[offset + 21];
                folderLength = bytes[offset + 22];
            }
            offset += header;
            if (flags & HasWorldHeight)
            {
                if (offset + 4 > bytes.size()) throw std::runtime_error(Tr("Incomplete marker height."));
                marker.worldHeight = std::bit_cast<float>(Read32(bytes, offset));
                offset += 4;
            }
            if (flags & HasWorldPosition)
            {
                if (offset + 12 > bytes.size()) throw std::runtime_error(Tr("Invalid world position."));
                marker.worldPosition = WorldVec3{std::bit_cast<float>(Read32(bytes, offset)),
                    std::bit_cast<float>(Read32(bytes, offset + 4)), std::bit_cast<float>(Read32(bytes, offset + 8))};
                offset += 12;
            }
            if (offset + nameLength + folderLength > bytes.size()) throw std::runtime_error(Tr("Incorrect marker code length."));
            marker.name.assign(reinterpret_cast<const char*>(bytes.data() + offset), nameLength);
            offset += nameLength;
            if (!folder)
            {
                marker.folderName.assign(reinterpret_cast<const char*>(bytes.data() + offset), folderLength);
                offset += folderLength;
            }
            return marker;
        }
    }

    bool ValidLabel(std::string_view text, size_t maxBytes, bool allowEmpty)
    {
        if (text.empty()) return allowEmpty;
        if (text.size() > maxBytes) return false;
        for (unsigned char c : text) if (c < 32 || c == 127) return false;
        return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0) > 0;
    }

    bool EncodeMarkerLink(const ShareableMarker& marker, std::string& link, std::string& error)
    {
        link.clear(); error.clear();
        try
        {
            if (!ValidPayload(marker, 127, 63))
                throw std::runtime_error(Tr("Cannot share this marker: check its name, folder, coordinates or height."));
            Bytes bytes;
            AppendMarkerRecord(bytes, marker, false);
            const auto digest = Digest(bytes);
            bytes.insert(bytes.end(), digest.begin(), digest.begin() + ChecksumSize);
            AppendEncoded(marker.worldPosition ? MarkerPrefixV3 : MarkerPrefixV2, bytes, link);
            if (link.size() > 256)
                throw std::runtime_error(Tr("The code exceeds 256 characters. Shorten the marker or folder name; no data was truncated."));
            return true;
        }
        catch (const std::exception& ex) { link.clear(); error = ex.what(); return false; }
    }

    bool DecodeMarkerLink(std::string_view link, ShareableMarker& marker, std::string& error)
    {
        error.clear();
        try
        {
            if (link.size() > 1024) throw std::runtime_error(Tr("Text is too long."));
            Trim(link);
            if (link.size() > 256) throw std::runtime_error(Tr("Marker code exceeds 256 characters."));
            const bool version3 = link.starts_with(MarkerPrefixV3);
            const bool version2 = version3 || link.starts_with(MarkerPrefixV2);
            if (!version2 && !link.starts_with(MarkerPrefixV1))
                throw std::runtime_error(Tr("Invalid code. It must start with VA1:, VA2: or VA3:."));
            const auto prefix = version3 ? MarkerPrefixV3 : version2 ? MarkerPrefixV2 : MarkerPrefixV1;
            Bytes bytes = DecodeEncoded(link.substr(prefix.size()));
            VerifyAndRemoveChecksum(bytes);
            size_t offset = 0;
            ShareableMarker decoded = DecodeMarkerRecord(bytes, offset, version2, false, version3);
            if (offset != bytes.size()) throw std::runtime_error(Tr("The marker code contains trailing data."));
            if (!ValidPayload(decoded, 127, 63)) throw std::runtime_error(Tr("The code contains invalid marker data."));
            marker = std::move(decoded);
            return true;
        }
        catch (const std::exception& ex) { error = ex.what(); return false; }
    }

    bool EncodeFolderLink(const ShareableFolder& folder, std::string& link, std::string& error)
    {
        link.clear(); error.clear();
        try
        {
            if (!ValidLabel(folder.name, 63, false) || folder.markers.size() > 20000)
                throw std::runtime_error(Tr("Cannot share this folder: check its name or contents."));
            Bytes bytes;
            Append16(bytes, static_cast<uint16_t>(folder.name.size()));
            Append32(bytes, static_cast<uint32_t>(folder.markers.size()));
            bytes.insert(bytes.end(), folder.name.begin(), folder.name.end());
            for (const auto& marker : folder.markers)
            {
                auto folderMarker = marker;
                folderMarker.folderName.clear();
                if (!ValidPayload(folderMarker, 4096, 0)) throw std::runtime_error(Tr("The folder contains an unshareable marker."));
                AppendMarkerRecord(bytes, marker, true);
                if (bytes.size() + ChecksumSize > MaxFolderPayloadSize)
                    throw std::runtime_error(Tr("The folder code exceeds 1 MiB. Split it into smaller folders to share it."));
            }
            const auto digest = Digest(bytes);
            bytes.insert(bytes.end(), digest.begin(), digest.begin() + ChecksumSize);
            const bool version3 = std::any_of(folder.markers.begin(), folder.markers.end(),
                [](const ShareableMarker& marker) { return marker.worldPosition.has_value(); });
            AppendEncoded(version3 ? FolderPrefixV3 : FolderPrefixV2, bytes, link);
            if (link.size() > MaxFolderCodeSize)
                throw std::runtime_error(Tr("The folder code exceeds 1 MiB. Split it into smaller folders to share it."));
            return true;
        }
        catch (const std::exception& ex) { link.clear(); error = ex.what(); return false; }
    }

    bool DecodeFolderLink(std::string_view link, ShareableFolder& folder, std::string& error)
    {
        error.clear();
        try
        {
            if (link.size() > MaxFolderCodeSize + 16) throw std::runtime_error(Tr("Folder code is too long."));
            Trim(link);
            const bool version3 = link.starts_with(FolderPrefixV3);
            const bool version2 = version3 || link.starts_with(FolderPrefixV2);
            if (!version2 && !link.starts_with(FolderPrefixV1))
                throw std::runtime_error(Tr("Invalid code. It must start with VAF1:, VAF2: or VAF3:."));
            if (link.size() > MaxFolderCodeSize) throw std::runtime_error(Tr("Folder code is too long."));
            const auto prefix = version3 ? FolderPrefixV3 : version2 ? FolderPrefixV2 : FolderPrefixV1;
            Bytes bytes = DecodeEncoded(link.substr(prefix.size()));
            VerifyAndRemoveChecksum(bytes);
            if (bytes.size() < FolderHeaderSize) throw std::runtime_error(Tr("Incomplete folder code."));
            const size_t folderNameLength = Read16(bytes, 0);
            const uint32_t markerCount = Read32(bytes, 2);
            if (folderNameLength == 0 || folderNameLength > 63 || markerCount > 20000 || FolderHeaderSize + folderNameLength > bytes.size())
                throw std::runtime_error(Tr("Invalid folder header."));
            ShareableFolder decoded;
            decoded.name.assign(reinterpret_cast<const char*>(bytes.data() + FolderHeaderSize), folderNameLength);
            if (!ValidLabel(decoded.name, 63, false)) throw std::runtime_error(Tr("Invalid folder name."));
            size_t offset = FolderHeaderSize + folderNameLength;
            decoded.markers.reserve(markerCount);
            for (uint32_t index = 0; index < markerCount; ++index)
            {
                ShareableMarker item = DecodeMarkerRecord(bytes, offset, version2, true, version3);
                if (!ValidPayload(item, 4096, 0)) throw std::runtime_error(Tr("The folder contains an invalid marker."));
                decoded.markers.push_back(std::move(item));
            }
            if (offset != bytes.size()) throw std::runtime_error(Tr("The folder contains trailing data."));
            folder = std::move(decoded);
            return true;
        }
        catch (const std::exception& ex) { error = ex.what(); return false; }
    }
}
