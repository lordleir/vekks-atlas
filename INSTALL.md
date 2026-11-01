# Install and use Vekk's Atlas 1.1.10

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
| Open or hide Pathing | Ctrl+Shift+N |

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

## Pathing

Enable Pathing mode using the pencil button next to Settings, or under **Settings → General**. Press **Ctrl+Shift+N** to show or hide its window. Choose **New Pathing Project** to name a new project, or **Select Pathing Project** to use an existing folder.

Choose a color and line style: continuous, dashed, dotted or arrows. Press **Record**, walk the route, then press **Stop**. Select a saved path to rename it or change its color and style. Dashed and dotted paths leave gaps for direction arrows. The end arrow is larger than the arrows along the path.

Recording ends when you change map or character, jump a large distance, or lose gameplay data for half a second. Shorter data interruptions add no points. Hiding the window does not stop recording.

Click one of the twelve compact marker icons to place a marker at your character and open its editor. The four additional roles are support, damage, tank and heal. This placement and route recording use MumbleLink; RTAPI is optional and remains the source for the separate cursor-placement shortcut in PvE.

The window has four tabs: **Record**, **Projects**, **Markers** and **Settings**. Use the path list to select a path; its eye and trash buttons hide or delete it. Projects contains sharing and strategy notes. Markers lists the project's saved markers.

To erase, activate the eraser and click a visible world path or marker. Confirm the deletion in the dialog. Press **Esc** to leave eraser mode. Use the path list to delete hidden paths.

**Copy project** shares the selected folder's paths, markers and strategy notes. Send the code to a friend, who can use **Paste project**. Imported projects appear on their original maps and belong to the importing character. Copies are independent. **Copy routes / Paste routes** also supports sharing only the current folder's completed paths on the current map.

In **Project details**, rename or hide the folder and add notes with timings and instructions. Deleting a project asks for confirmation and removes its paths and markers.

Use **Settings → Routes** to choose where paths appear and adjust opacity, world width, map line width, glow, arrows and animation. WvW supports world display only.

Route recording is limited to 128 routes, 4,096 samples per route and 65,536 samples in total. Recording stops when a limit is reached. Routes follow sampled height and may appear through terrain.

If a path flickers or recording ends unexpectedly, look for `PathingDiag:` in `addons/Nexus/Nexus.log`. Entries include stop reasons, data interruptions, drawing limits and save errors. Identical messages are limited to one every ten seconds. Logs can contain character names; review them before sharing.

## Language and saved data

English, Español and Deutsch are available in settings. Custom names are unchanged. Settings apply across characters and can be shared as a text code; markers and folders are per character.

Data is saved in `addons/VekksAtlas/markers.json`. Each save keeps a `.bak` copy. If the file cannot be read, Atlas leaves it intact and blocks saving rather than overwrite it.

The same file stores your paths and project notes. Keep a backup before moving or manually editing it.
