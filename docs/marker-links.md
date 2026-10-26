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

Atlas still accepts legacy `VA1:` codes. They have the same basic location data but no 3D elevation.

Markers with a saved MumbleLink world position use `VA3:`. This extends the VA2 record with flag bit 1 and three float32 values (X, Y, Z in metres) after the optional elevation and before the names. Y is vertical. The original position and manual elevation are preserved separately. The 256-character limit still applies.

## Folder: VAF2

`VAF2:` stores a folder name and all its markers, including optional 3D elevation. Character names and local IDs are never included. The limits are 1 MiB and 20,000 markers.

If an imported folder has the same name as an existing folder, Atlas asks whether to replace it or create a separate copy. Replacing it replaces its marker contents; creating a copy generates a unique folder name.

Legacy `VAF1:` folders remain importable.

Folders containing saved world positions use `VAF3:` with the same extended marker records. VA1/VA2 and VAF1/VAF2 codes remain supported. These formats are supported by Atlas 1.0.

## Security and import behavior

Checksums detect damaged codes. They do not verify the sender and they do not encrypt anything. Neither format contains account or character identity.

Imports preserve the original map position and, when present, the 3D elevation. Shared copies are local and are not synchronized with the sender.
