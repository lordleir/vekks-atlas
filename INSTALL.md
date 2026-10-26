# Install and use Vekk's Atlas

Requires Windows x64, Guild Wars 2 and Nexus.

## Installation

1. Download `VekksAtlas.dll` from the repository's releases.
2. Disable Atlas in Nexus, or close Guild Wars 2.
3. Copy the DLL into `Guild Wars 2/addons/` and enable Atlas in Nexus.

Do not replace Nexus's `d3d11.dll`. Keep `addons/VekksAtlas/markers.json`; it contains your markers and settings.

## Controls

| Action | Default control |
| --- | --- |
| Create a map marker in PvE | Right-click the open world map |
| Edit a map marker in PvE | Right-click its icon |
| Delete a minimap marker in PvE | Right-click its icon |
| Place at your character in WvW | Ctrl+Shift+M |
| Place at the world cursor in PvE, with advanced mode | Ctrl+Shift+M |
| Open settings | Ctrl+Shift+A or the Atlas launcher |
| Open or hide raid tools | Ctrl+Shift+N |

Change shortcuts inside **Settings → Shortcuts**. The launcher can be resized and repositioned there and through Atlas's Nexus options.

## WvW

Stand at the desired location, press the placement shortcut and save. WvW markers appear only in the 3D world. To edit one, open the placement window, open Folders, select a marker on the current map and click **Edit marker**. Its position and elevation can be adjusted there.

## Advanced cursor placement

Install and enable RTAPI in Nexus, then enable **Advanced mode** in Atlas. This optional feature requires RTAPI's game-memory access. WvW character placement does not require RTAPI.

## Collections

Use the folder button to create, rename, hide, copy or delete collections. When deleting a folder, you can move its markers or delete them. Importing a duplicate folder name offers replacement or a separate copy.

**Copy Marker** and **Copy folder** produce text codes for sharing. Paste them in the matching import field. Imported copies retain their saved coordinates and elevation; they are not synchronized with the sender.

## Light pillars

The Light pillars tab controls intensity, width, range, world icons, names, distances and opacity. Icons and labels become smaller with camera depth while remaining readable.

Map placement cannot detect terrain height. Use **World elevation → Use my elevation** while standing at the desired height, or enter the height manually. Pillars may appear through terrain.

## Raid tools

Enable Raid mode using the crossed-swords button next to Settings, or under **Settings → General**. Press **Ctrl+Shift+N** to show or hide its toolbar in the 3D world.

The pencil records your movement from the point where you start. Click the stop icon to finish. Paths are drawn as dashed lines with direction arrows. Recording ends on map/character changes, unavailable gameplay data or a large position jump. Hiding the toolbar does not stop recording; open it again to stop.

Click one of the twelve compact marker icons to place a marker at your character and open its editor. The four additional roles are support, damage, tank and heal. This placement and route recording use MumbleLink; RTAPI is optional and remains the source for the separate cursor-placement shortcut in PvE.

The eraser selects visible world paths or marker icons, then asks for confirmation. The route list also allows deletion, renaming, recoloring and visibility changes.

**Copy routes** copies all completed routes on the current map. Send the code to a friend, who can use **Paste routes**. Imported routes appear on their original map and belong to the importing character. Markers can be shared using the existing folder codes. Routes and markers are local copies, not live synchronized drawings.

Route recording is limited to 128 routes, 4,096 samples per route and 65,536 samples in total. Recording stops when a limit is reached. Routes follow sampled height and may appear through terrain.

## Language and saved data

English, Español and Deutsch are available in settings. Custom names are unchanged. Settings apply across characters and can be shared as a text code; markers and folders are per character.

Data is saved in `addons/VekksAtlas/markers.json`. Each save keeps a `.bak` copy. If the file cannot be read, Atlas leaves it intact and blocks saving rather than overwrite it.

Version 1.1 also stores routes in this file. It reads previous marker files; older DLLs cannot read the new route-aware file format. Keep a backup before downgrading.
