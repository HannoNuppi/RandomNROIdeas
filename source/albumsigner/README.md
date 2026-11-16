# AlbumSigner image and video-preflight implementation

The current image path is implemented in \`../albumsigner.c\` and \`../image_decode.c\`. The MP4 inspection path is implemented in \`../video_probe.c\`.

## Current behavior

1. Requires Application mode; applet-mode launches are rejected.
2. Mounts the SD card and browses for JPEG, PNG, or MP4 files.
3. Enumerates installed titles and reads their localized display names and application Title IDs.
4. Shows the selected media, selected game, and current console-local time.
5. For JPEG/PNG, decodes the input locally and fits it into an opaque 1280x720 RGBA canvas, preserving aspect ratio with black bars where necessary.
6. For images, sets the screenshot identity to the selected installed application and calls libnx's \`capssuSaveScreenShot()\` system service.
7. For MP4, reads only ISO BMFF box and track metadata: container structure, video/audio sample-entry type, dimensions, duration, fragmented layout, and fast-start layout. It does not decode, transcode, remux, or modify the source.
8. Video preflight currently recognizes a conservative first input target: non-fragmented H.264/AVC at 1280x720, with AAC audio or no audio. Pressing the action button reports the analysis result but deliberately does not write an Album video.

## Dependencies

- libnx from devkitPro
- \`stb_image.h\` from the pinned \`nothings/stb\` commit. The Makefile downloads it on the first build and caches it locally; the built NRO has no runtime network dependency.

Run \`make\` in the devkitPro MSYS2 shell. A first build requires internet access only to fetch this pinned header.

## Video import still outstanding

An ordinary MP4 is not an authenticated Switch Album movie. A valid movie entry also needs a JPEG thumbnail, a 0x498-byte MakerNote with encrypted movie metadata, a movie CMAC, and SHA-256 hashes covering each 0x40000-byte (256-KiB) chunk.

libnx exposes the MovieMaker capture path for a running application, but that saves an ExtraMovie separately from normal Album entries. There is no generic libnx wrapper that accepts an arbitrary MP4 and imports it into the normal Album. The full import writer therefore remains disabled until its data layout and system-crypto path have been verified.

## Important compatibility notes

- The native Album screenshot save service \`caps:su\` is supported on system version 4.0.0 and newer.
- The screenshot capture API accepts a 1280x720 RGBA8 buffer. The NRO decodes JPEG/PNG inputs locally and resizes them before handing them to the service.
- The selected-game attribution is requested through \`appletSetScreenShotAppletIdentityInfo()\`. This must still be verified on a real Switch; successful compilation alone does not prove the Album will attribute the entry to the chosen game.
- The MP4 preflight is a structural inspection only. It does not decode H.264 samples, inspect every sample table entry, extract a thumbnail, or prove that the Album decoder accepts the file.
- The app does not extract or store Nintendo's private screenshot/movie keys. The official screenshot service handles image Album authentication; no video signing is currently performed.

See [the full design](../../ideas/01-albumsigner.md).
