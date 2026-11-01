# Marker sharing formats

Atlas shares markers as text codes. They are not web links or GW2 chat links.

## Individual marker: VA2

`VA2:` is followed by canonical unpadded Base64URL and is limited to **256 ASCII characters**. The payload contains:

- map, continent and floor IDs;
- continent X and Y coordinates;
- the icon;
- marker and folder names;
- optional 3D world elevation;
- the first eight bytes of a SHA-256 checksum.

The icon byte supports the eight general icons plus portal, arrow cart, catapult, trebuchet, flame ram and shield generator. Marker names may use 127 UTF-8 bytes and folder names 63, provided the complete code remains within 256 characters. Atlas never truncates data silently.

Support, damage, tank and heal use icon IDs 14–17.

Atlas still accepts legacy `VA1:` codes. They have the same basic location data but no 3D elevation.

Markers with a saved MumbleLink world position use `VA3:`. This extends the VA2 record with flag bit 1 and three float32 values (X, Y, Z in metres) after the optional elevation and before the names. Y is vertical. The original position and manual elevation are preserved separately. The 256-character limit still applies.

## Folder: VAF2

`VAF2:` stores a folder name and all its markers, including optional 3D elevation. Character names and local IDs are never included. The limits are 1 MiB and 20,000 markers.

If an imported folder has the same name as an existing folder, Atlas asks whether to replace it or create a separate copy. Replacing it replaces its marker contents; creating a copy generates a unique folder name.

Legacy `VAF1:` folders remain importable.

Folders containing saved world positions use `VAF3:` with the same extended marker records. VA1/VA2 and VAF1/VAF2 codes remain supported.

## Security and import behavior

Checksums detect damaged codes. They do not verify the sender and they do not encrypt anything. Neither format contains account or character identity.

Imports preserve the original map position and, when present, the 3D elevation. Shared copies are local and are not synchronized with the sender.

## Routes: VAR1

`VAR1:` contains a JSON array of routes with map ID, name, color, visibility and XYZ samples in MumbleLink metres. An optional `map_anchor` pairs a world XYZ position with its continent XY position for PvE map projection. The JSON uses ASCII escapes for non-ASCII names. It contains no character names or local IDs. Import generates new IDs and assigns routes to the receiving character's selected project folder.

The maximum text size is 1 MiB. A route has 2–4,096 finite samples; adjacent points must be at most 30 metres apart. Storage is limited to 128 routes and 65,536 total samples. Imports validate all routes before appending any of them. Route codes are plain editable JSON, without a checksum or sender authentication.

## Settings

`VAS2:` includes pillar width and accepts a range starting at 250 displayed units. `VAS1:` remains importable with the default width. Paths and their appearance settings are saved separately and are not included in settings codes.

## Pathing projects — VAD1

`VAD1:` contains an ASCII-escaped JSON object with `folder` (a VAF folder code containing markers), `paths` (the route array described above), and `notes` (up to 4,096 UTF-8 bytes). The whole code is limited to 1 MiB. Import assigns fresh IDs and a shared destination folder to all contents. Duplicate folder names offer replacement or a separate copy. The outer project code is editable JSON without a checksum; the embedded folder code retains its own integrity check.

Local storage schema 5 adds `drawing` and `notes` to folders, `folder_id` and optional `map_anchor` to paths, and route appearance settings. Schema versions 1–4 remain readable. Old unfiled paths are assigned to a project folder when their character is active. The stored `drawing` field identifies a Pathing project.

Path codes and local paths may include `style`: 0 (continuous), 1 (dashed), 2 (dotted), or 3 (arrows). Missing values default to continuous for older saves and share codes.
