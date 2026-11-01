#include "MarkerLink.h"
#include "AtlasLocalization.h"
#include "AtlasShortcut.h"

#include <cmath>
#include <iostream>

namespace
{
    int Fail(const char* message)
    {
        std::cerr << message << '\n';
        return 1;
    }
}

int main()
{
    using namespace cmm;

    if (UiLabel("On map##distance") == UiLabel("On minimap##distance") ||
        UiLabel("On map##distance") == UiLabel("In 3D world##distance"))
        return Fail("Distance checkboxes share an ImGui identifier.");
    if (UiLabel("On map##name") == UiLabel("On minimap##name") ||
        UiLabel("On map##name") == UiLabel("In 3D world##name"))
        return Fail("Name checkboxes share an ImGui identifier.");

    const Shortcut placement{'M', false, true, true};
    if (!MatchesShortcut(placement, 'M', false, true, true) ||
        MatchesShortcut(placement, 'M', false, true, false) ||
        MatchesShortcut(placement, 'N', false, true, true))
        return Fail("Shortcut matching does not require the exact key and modifiers.");
    if (ShortcutText(placement) != "Ctrl+Shift+M")
        return Fail("Shortcut display text is incorrect.");
    if (IsModifierKey(VK_CONTROL) == false || IsModifierKey('M'))
        return Fail("Modifier keys are not classified correctly.");
    const auto clamped = ClampLauncherPosition(-25.0f, 900.0f, 40.0f, 800.0f, 600.0f);
    if (std::abs(clamped.first) > 0.001f || std::abs(clamped.second - 560.0f) > 0.001f)
        return Fail("Launcher position is not clamped inside the display.");

    ShareableMarker source{};
    source.mapId = 38;
    source.continentId = 2;
    source.floorId = 3;
    source.continent = {1234.5f, 6789.25f};
    source.icon = Icon::Portal;
    source.name = "Portal north";
    source.folderName = "WvW";
    source.worldHeight = 42.75f;

    std::string code;
    std::string error;
    if (!EncodeMarkerLink(source, code, error)) return Fail(error.c_str());
    if (!code.starts_with("VA2:")) return Fail("New marker code does not use VA2.");

    ShareableMarker decoded{};
    if (!DecodeMarkerLink(code, decoded, error)) return Fail(error.c_str());
    if (!decoded.worldHeight || std::abs(*decoded.worldHeight - 42.75f) > 0.001f)
        return Fail("Marker height was not preserved.");
    if (decoded.icon != Icon::Portal || decoded.name != source.name || decoded.folderName != source.folderName)
        return Fail("Marker fields changed during round trip.");

    ShareableFolder folder{"Siege", {source}};
    if (!EncodeFolderLink(folder, code, error)) return Fail(error.c_str());
    if (!code.starts_with("VAF2:")) return Fail("New folder code does not use VAF2.");
    ShareableFolder decodedFolder{};
    if (!DecodeFolderLink(code, decodedFolder, error)) return Fail(error.c_str());
    if (decodedFolder.markers.size() != 1 || !decodedFolder.markers.front().worldHeight ||
        std::abs(*decodedFolder.markers.front().worldHeight - 42.75f) > 0.001f)
        return Fail("Folder marker height was not preserved.");

    source.worldHeight.reset();
    if (!EncodeMarkerLink(source, code, error) || !DecodeMarkerLink(code, decoded, error) || decoded.worldHeight)
        return Fail("Absent marker height did not remain absent.");

    return 0;
}
