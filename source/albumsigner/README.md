# AlbumSigner implementation area

AlbumSigner is designed as an offline, Application-mode NRO. It must refuse to process media when launched as an applet.

## Planned modules

- `app_mode.c` — check `appletGetAppletType() == AppletType_Application` and display a clear launch-mode error otherwise.
- `title_catalog.c` — enumerate installed games via NS services and read each localized title/icon.
- `file_browser.c` — browse SD files without loading large media fully into memory.
- `album_clock.c` — obtain the current local console timestamp.
- `image_pipeline.c` — image decode/resize and Album JPEG/EXIF/MakerNote output.
- `video_pipeline.c` — MP4 input, thumbnail, encrypted video metadata, and streaming chunk hashes.
- `album_crypto.c` — firmware/platform-specific crypto backends using supported system services; no embedded private key material.
- `output_writer.c` — temporary output, validation, collision-safe naming, and commit to `Nintendo/Album/YYYY/MM/DD/`.

## User flow

Select an SD media file → select an installed game → use the current console time → add a new Album entry. The original file remains untouched, and the process is fully offline.

The first milestone is the Application-mode shell, SD browser, title picker, clock, and dry-run output path. It should not claim to sign files until the image/video format and authentication implementation are validated on real hardware.

See [the full design](../../ideas/01-albumsigner.md) for requirements and compatibility milestones.
