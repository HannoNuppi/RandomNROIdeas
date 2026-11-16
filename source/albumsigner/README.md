# AlbumSigner implementation area

The planned native implementation will be split into small components:

- `title_catalog.c` — enumerate installed titles and read localized game names using libnx NS services.
- `file_browser.c` — browse SD files without loading them fully into memory.
- `image_pipeline.c` — decode/resize images and build Nintendo-compatible JPEG/EXIF output.
- `video_pipeline.c` — MP4 processing, thumbnail creation, and streaming hash generation.
- `album_crypto.c` — firmware-aware MAC and movie metadata operations; no embedded secret material.
- `output_writer.c` — collision-safe timestamp naming and atomic staging/final writes.

The feature is intentionally not wired into the main menu yet: the cryptographic format and Switch 2 compatibility need verified test fixtures before a UI can safely claim to produce valid Album files.

See [the full design](../../ideas/01-albumsigner.md) for requirements and implementation milestones.
