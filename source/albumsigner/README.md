# AlbumSigner image implementation

The initial image-import path is implemented in `../albumsigner.c` and `../image_decode.c`.

## Current behavior

1. Requires Application mode; applet-mode launches are rejected.
2. Mounts the SD card and browses for JPEG or PNG images.
3. Enumerates installed titles and reads their localized display names and application Title IDs.
4. Shows the selected image, selected game, and current console-local time.
5. On user confirmation, decodes the input image locally and fits it into an opaque 1280x720 RGBA canvas, preserving the aspect ratio with black bars where necessary.
6. Sets the screenshot identity to the selected installed application and calls libnx's `capssuSaveScreenShot()` system service.
7. The Switch system service creates the Album screenshot entry and generates its own Album metadata/authentication. The original file is not modified.

## Dependencies

- libnx from devkitPro
- `stb_image.h` from the pinned `nothings/stb` commit. The Makefile downloads it on the first build and caches it locally; the built NRO has no runtime network dependency.

Run `make` in the devkitPro MSYS2 shell. A first build requires internet access only to fetch this pinned header.

## Important compatibility notes

- The native Album save service `caps:su` is supported on system version 4.0.0 and newer.
- The capture API accepts a 1280x720 RGBA8 buffer. The NRO decodes JPEG/PNG inputs locally and resizes them before handing them to the service.
- The selected-game attribution is requested through `appletSetScreenShotAppletIdentityInfo()`. This must still be verified on a real Switch; successful compilation alone does not prove the Album will attribute the entry to the chosen game.
- This phase only supports still images. Video conversion and Album movie metadata are not implemented.
- The app does not extract or store Nintendo's private screenshot keys. The official system service handles the Album authentication.

See [the full design](../../ideas/01-albumsigner.md).
