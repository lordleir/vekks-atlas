#pragma once

#include "AtlasLocalization.h"
#include "AtlasShortcut.h"

namespace cmm
{
    struct Preferences
    {
        Language language{Language::English};
        bool nameMap{true};
        bool nameMinimap{true};
        bool nameWorld{true};
        bool distanceMap{false};
        bool distanceMinimap{false};
        bool distanceWorld{false};
        float mapIconSize{32.0f};
        float minimapIconSize{24.0f};
        float labelScale{1.0f};
        float iconOpacity{1.0f};
        float nameOpacity{1.0f};
        float distanceOpacity{1.0f};
        bool edgeIndicators{true};
        bool iconEffects{true};
        bool labelBackground{true};
        bool shareHelp{true};
        bool worldBeacons{true};
        float beaconOpacity{0.70f};
        float beaconMaxDistance{2500.0f};
        float beaconWidth{1.0f};
        bool worldIcons{true};
        float worldIconScale{1.0f};
        float worldTextScale{1.0f};
        float worldIconOpacity{1.0f};
        float worldNameOpacity{1.0f};
        float worldDistanceOpacity{1.0f};
        bool advancedMode{false};
        bool launcherVisible{true};
        float launcherSize{36.0f};
        float launcherX{54.0f};
        float launcherY{8.0f};
        Shortcut placementShortcut{'M', false, true, true};
        Shortcut settingsShortcut{'A', false, true, true};
    };
}
