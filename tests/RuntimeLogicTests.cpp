#include "AdvancedPlacement.h"
#include "AtlasColors.h"
#include "WorldReference.h"
#include "WorldBeacons.h"
#include "SettingsLink.h"
#include "CompetitiveCoordinates.h"

#include <cmath>
#include <iostream>

namespace
{
    int Fail(const char* message)
    {
        std::cerr << message << '\n';
        return 1;
    }

    bool Near(float a, float b, float tolerance = 0.01f)
    {
        return std::abs(a - b) <= tolerance;
    }
}

int main()
{
    using namespace cmm;

    // RTAPI is current while MumbleLink can lag several metres during movement.
    // Placement must use relative camera/facing vectors, not absolute positions,
    // or the inferred axis permutation can change while the player walks.
    AdvancedCursorSnapshot moving{};
    moving.gameBuild = 1;
    moving.mapId = 15;
    moving.gameplay = true;
    moving.characterPosition = {1214.0796f, 5704.9175f, 5887.2720f};
    moving.cameraPosition = {1016.5118f, 5795.7944f, 5889.0356f};
    moving.cursor = {1314.0796f, 5704.9175f, 5887.2720f};
    moving.cameraFacing = {0.9f, 0.1f, 0.4f};

    const WorldMapReference laggedMumble{{24.9799f, 149.5367f, -154.2132f}, 1000.0f, 2000.0f};
    const WorldVec3 mumbleCamera{19.9616f, 149.5815f, -156.5215f};
    // True RTAPI -> Mumble mapping: {x,z,-y} * 0.0254.
    const WorldVec3 mumbleFront{0.9117f, 0.4052f, -0.1013f};

    AdvancedPlacementError error{};
    const auto placement = ConvertAdvancedCursor(moving, 15, laggedMumble, mumbleCamera, mumbleFront, error);
    if (!placement || error != AdvancedPlacementError::None)
        return Fail("Moving placement was rejected.");
    if (!Near(placement->continent.x, 1004.1667f) || !Near(placement->continent.y, 2000.0f) ||
        !Near(placement->worldHeight, laggedMumble.worldPosition.y))
        return Fail("Moving placement changed the cursor location.");

    WorldReferenceTracker references;
    WorldBeaconCamera firstCamera{};
    firstCamera.avatarPosition = {10.0f, 20.0f, 30.0f};
    firstCamera.playerContinentX = 500.0f;
    firstCamera.playerContinentY = 700.0f;
    const auto firstReference = references.GetOrCreate(15, "Character", firstCamera);

    WorldBeaconCamera movingCamera = firstCamera;
    movingCamera.avatarPosition = {40.0f, 20.0f, 65.0f};
    movingCamera.playerContinentX = 549.2126f;
    movingCamera.playerContinentY = 642.5853f;
    const auto movingReference = references.GetOrCreate(15, "Character", movingCamera);
    if (!Near(movingReference.worldPosition.x, firstReference.worldPosition.x) ||
        !Near(movingReference.worldPosition.z, firstReference.worldPosition.z) ||
        !Near(movingReference.continentX, firstReference.continentX) ||
        !Near(movingReference.continentY, firstReference.continentY))
        return Fail("World reference followed the moving character.");

    const auto changedMapReference = references.GetOrCreate(16, "Character", movingCamera);
    if (!Near(changedMapReference.worldPosition.x, movingCamera.avatarPosition.x) ||
        !Near(changedMapReference.continentX, movingCamera.playerContinentX))
        return Fail("World reference did not reset after changing maps.");

    if (MarkerColor(Icon::Portal) != Rgba(191, 80, 232) ||
        MarkerColor(Icon::ArrowCart) != Rgba(108, 190, 59) ||
        MarkerColor(Icon::Catapult) != Rgba(225, 154, 45) ||
        MarkerColor(Icon::Trebuchet) != Rgba(45, 125, 235) ||
        MarkerColor(Icon::FlameRam) != Rgba(218, 72, 49) ||
        MarkerColor(Icon::ShieldGenerator) != Rgba(72, 205, 205))
        return Fail("A WvW beacon color does not match its icon.");

    const auto eternalCenter = CompetitivePlayerContinent(38, {0.0f, 25.0f, 0.0f});
    if (!eternalCenter || !Near(eternalCenter->x, 10494.0f) || !Near(eternalCenter->y, 14334.0f))
        return Fail("WvW world position was not converted through the official map rectangles.");
    const auto eternalOffset = CompetitivePlayerContinent(38, {60.96f, 25.0f, -60.96f});
    if (!eternalOffset || !Near(eternalOffset->x, 10594.0f) || !Near(eternalOffset->y, 14434.0f))
        return Fail("WvW coordinate axes or scale are incorrect.");
    if (CompetitivePlayerContinent(999999, {}).has_value())
        return Fail("Unknown competitive map unexpectedly has a coordinate transform.");

    if (!Near(WorldUnitsToMetres(1000.0f), 609.6f) || !Near(MetresToWorldUnits(609.6f), 1000.0f))
        return Fail("World-unit conversion is incorrect.");
    const auto strong = ComputeWorldBeaconAlphas(100.0f, 1000.0f, 1.0f);
    const auto dim = ComputeWorldBeaconAlphas(100.0f, 1000.0f, .25f);
    if (!Near(strong.annotation, dim.annotation) || !Near(dim.beam, strong.beam*.25f))
        return Fail("Beam intensity still changes annotation opacity.");

    Preferences preferences{};
    preferences.language = Language::Spanish;
    preferences.distanceMinimap = true;
    preferences.distanceWorld = true;
    preferences.worldIconScale = 1.35f;
    preferences.worldTextScale = 1.20f;
    preferences.worldIconOpacity = .75f;
    preferences.launcherX = 0.0f;
    std::string settingsCode, settingsError;
    if (!EncodeSettingsLink(preferences, settingsCode, settingsError) ||
        !settingsCode.starts_with("VAS2:") || settingsCode.size() > 256)
        return Fail("Settings code was not encoded compactly.");
    Preferences decodedPreferences{};
    if (!DecodeSettingsLink(settingsCode, decodedPreferences, settingsError) ||
        decodedPreferences.language != Language::Spanish || !decodedPreferences.distanceMinimap ||
        !decodedPreferences.distanceWorld || !Near(decodedPreferences.worldIconScale, 1.35f) ||
        !Near(decodedPreferences.worldIconOpacity, .75f) || decodedPreferences.launcherX != 0.0f)
        return Fail("Settings changed during round trip.");
    settingsCode.back() = settingsCode.back() == 'A' ? 'B' : 'A';
    const Preferences unchanged = decodedPreferences;
    if (DecodeSettingsLink(settingsCode, decodedPreferences, settingsError) ||
        decodedPreferences.language != unchanged.language)
        return Fail("Damaged settings code was accepted or changed settings.");

    return 0;
}
