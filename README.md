# Vekk's Atlas

Personal markers for Guild Wars 2, by **Leir.6218**. Requires [Nexus](https://raidcore.gg/Nexus).

Save places on the map, organize them into folders and share them with friends. In PvE, markers appear on the world map, minimap and as optional 3D light pillars. In WvW, markers use your character's saved 3D position and appear in the world only.

## Install

Download the [1.1 raid preview](https://github.com/lordleir/vekks-atlas/releases/tag/v1.1.0) or the [1.0 base release](https://github.com/lordleir/vekks-atlas/releases/tag/v1.0.0). Version 1.1 has been compiled; in-game validation is pending.

Place `VekksAtlas.dll` in `Guild Wars 2/addons/`. Disable Atlas in Nexus before replacing the DLL, then enable it again. Windows x64 is required.

Your saved markers are kept when updating. Icons and translations are included in the DLL.

## Place a marker

- **PvE:** open the world map with **M**, right-click a location and choose an icon, name and folder.
- **WvW:** stand where you want the marker and press **Ctrl+Shift+M**. Atlas saves your character's position, including height.
- **Optional PvE cursor placement:** install [RTAPI](https://raidcore.gg/gw2/addons/rtapi), enable **Advanced mode** and press **Ctrl+Shift+M** while pointing at the world.

Change shortcuts inside Atlas settings. **Ctrl+Shift+A** opens settings by default.

Right-click a map marker to edit it, or a minimap marker to delete it. In WvW, open the placement window, select the folder button, select a saved marker and choose **Edit marker**.

## Organize and share

Folders can be hidden, renamed, copied and imported. Deleting a folder lets you move its markers elsewhere or delete them. Importing an existing folder name asks whether to replace it or create a separate copy.

**Copy Marker** copies a text code you can send to a friend. Pasting it recreates the same marker, including saved elevation. Folder codes share complete collections. Shared copies are independent.

## Raid tools — 1.1

Enable **Raid mode** with the crossed-swords button beside Settings, or in **Settings → General**. **Ctrl+Shift+N** opens or hides its compact toolbar. The marker picker gains four roles: support, damage, tank and heal.

- Click the pencil to start recording from your character's current position. Walk the route, then click the stop button.
- Saved routes appear as dashed 3D lines with arrows in the direction you walked.
- Click an icon in the toolbar to place a marker at your feet and open its editor.
- Select the eraser, click a visible route or marker, and confirm before deleting it.
- Name, recolor, hide or delete routes from the toolbar's list.
- **Copy routes** shares the routes on your current map. A friend can use **Paste routes** to import the code. Share role markers separately using marker folders.

Recording uses MumbleLink and does not require RTAPI. Map changes, loading and large position jumps stop recording. Routes follow your sampled position, including jumps; they do not detect terrain and can appear through walls. The toolbar's visibility does not affect saved routes; disabling Raid mode hides the routes.

## Appearance

Choose which names and distances appear on the map, minimap or light pillars. Adjust icon size, opacity, pillar intensity, width and range. World icons and labels scale with camera depth. Minimap edge indicators point toward locations outside the visible area.

English is the default; **Español** and **Deutsch** are available in settings. You can also copy your settings as a text code.

## Limitations and data

WvW map and minimap overlays are disabled because their live map center is unavailable through the supported data interfaces. WvW world markers remain anchored to their saved position.

Map clicks do not provide terrain height. Use the marker's elevation control when needed. Light pillars are overlays and may appear through terrain or walls.

Markers, folders and settings are saved locally in `addons/VekksAtlas/markers.json`, with a backup of the previous save. Markers and folders belong to each character; settings apply across characters.

Atlas uses MumbleLink shared telemetry and Nexus. It does not scan internal game memory. Optional advanced cursor placement consumes data from RTAPI, a separate addon that does use game-memory access.

**AI Notice:** developed by Leir.6218 with AI assistance.

Not affiliated with or endorsed by ArenaNet or Raidcore. [Installation guide](INSTALL.md) · [Third-party notices](DEPENDENCIES.md)

## Video

[Watch Vekk's Atlas in action](https://youtu.be/1x_KMD0CM6I)
