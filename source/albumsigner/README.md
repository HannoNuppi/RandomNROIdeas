# AlbumSigner implementation

The first native prototype is implemented in `../albumsigner.c`.

## Currently implemented

- Require `AppletType_Application`; applet-mode launches are rejected.
- Mount the SD card with libnx's `fsdevMountSdmc()`.
- Browse folders and select JPG, JPEG, PNG, MP4, MOV, and M4V media.
- Query installed application records with NS services.
- Read localized application names and show the 64-bit application Title ID.
- Read the console's current user-system clock and convert it to the configured local timezone.
- Preview the date-based Album destination folder without writing any files.

## Not implemented yet

- Album-specific Title ID mapping.
- JPEG re-encoding and EXIF/MakerNote writing.
- Screenshot MAC generation.
- MP4 conversion, preview generation, encrypted movie metadata, and per-chunk hashes.
- Final collision-safe Album file creation.

The prototype intentionally leaves all input files unchanged and clearly labels its last screen as a preview. A successful CI build proves only that the NRO compiles and its file header passes the smoke checks; it does not prove compatibility with the console's Album viewer.

See [the full design](../../ideas/01-albumsigner.md).
