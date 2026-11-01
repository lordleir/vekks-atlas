# Dependencies and assets

## Embedded third-party code

- **ImGui 1.80:** from RaidcoreGG/Nexus revision `47a5b220e1f173624dd494190c3d3f2f17c8dd5d`. MIT license: `external/imgui/LICENSE.txt`.
- **Nexus API 6:** pinned public header in `external/Nexus.h`; license in `external/Nexus-API-LICENSE.txt`.
- **MumbleLink:** public RaidcoreGG/gw2-mumble-cpp header in `external/Mumble.h`; license in `external/Mumble-LICENSE.txt`.
- **GW2 RealTime API (optional):** public header in `external/RTAPI.hpp`; license in `external/RTAPI-LICENSE.txt`. RTAPI itself is not redistributed.
- **nlohmann/json 3.12.0:** `external/nlohmann/json.hpp`; MIT license in the same folder.
- License texts are also embedded in the DLL.
- **Windows BCrypt / User32:** used for checksums and clipboard access.

## Assets

- Icon originals are in `assets/icons-source/`; 64/128 px versions are in `assets/icons/`.
- Map-name catalogs fetched from ArenaNet's public API on 2026-10-24, one for each supported language, reduced to `id` and `name`:
  - https://api.guildwars2.com/v2/maps?ids=all&lang=en
  - https://api.guildwars2.com/v2/maps?ids=all&lang=es
  - https://api.guildwars2.com/v2/maps?ids=all&lang=de
- The catalogs are embedded in the DLL.

## References

- [Nexus API builder](https://github.com/RaidcoreGG/Nexus/blob/47a5b220e1f173624dd494190c3d3f2f17c8dd5d/Nexus/Host/Addons/API/ApiBuilder.cpp)
- [GW2TacO MumbleLink](https://github.com/boyc/GW2TacO/blob/master/MumbleLink.cpp)
- [Mumble Link documentation](https://www.mumble.info/documentation/developer/positional-audio/link-plugin/)
- [Nexus Pathing](https://github.com/YoruDev-Ryland/GW2-Nexus---Pathing)
- [GW2 API Maps](https://wiki.guildwars2.com/wiki/API:Maps)
- [GW2 RealTime API](https://github.com/gwdevcommunity/GW2-RealTime-API-Releases)
