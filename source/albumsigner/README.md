# AlbumSigner image and video pipeline

The image path is implemented in `../albumsigner.c` and `../image_decode.c`. MP4 inspection is implemented in `../video_probe.c`. The streaming backend is implemented in `../album_video_writer.c` and `../album_video_caps_backend.c`.

## Current behavior

1. Requires Application mode; applet-mode launches are rejected.
2. Mounts the SD card and browses for JPEG, PNG, or MP4 files.
3. Enumerates installed titles and reads their localized display names and application Title IDs.
4. Shows the selected media, selected game, and current console-local time.
5. For JPEG/PNG, decodes the input locally and fits it into an opaque 1280x720 RGBA canvas, preserving aspect ratio with black bars where necessary.
6. For images, sets the screenshot identity to the selected installed application and calls libnx's `capssuSaveScreenShot()` service.
7. For MP4, inspects ISO BMFF box and track metadata: container structure, video/audio sample-entry type, dimensions, duration, fragmented layout, and fast-start layout. It does not decode, transcode, remux, or modify the source.
8. Video preflight currently recognizes a conservative first input target: non-fragmented H.264/AVC at 1280x720, with AAC audio or no audio. Pressing the action button reports the analysis result but deliberately does not write an Album video.
9. The separate video writer module can stream an MP4 and a prebuilt Album movie metadata section through libnx's `caps:c` `AlbumMovieWriteStream` interface. It uses 256-KiB buffers, sets the MP4 data size, writes video and metadata sections, then calls Finish and Commit. Failed writes discard the temporary stream.

## Dependencies

- libnx from devkitPro
- `stb_image.h` from the pinned `nothings/stb` commit. The Makefile downloads it on the first build and caches it locally; the built NRO has no runtime network dependency.

Run `make` in the devkitPro MSYS2 shell. A first build requires internet access only to fetch this pinned header.

## Video import still outstanding

The official Album service exposes a generic write stream for normal Movie entries. The service can generate a current-time Movie file identity for an application, and the write stream has separate data and metadata sections. The current backend uses those APIs, enforces the 2-GiB movie-data ceiling, and commits through the system service.

The backend is intentionally not called by the AlbumSigner UI yet. A normal MP4 is not a complete Album movie package. The system expects a JPEG thumbnail with EXIF MakerNote, a 0x498-byte MakerNote containing encrypted movie metadata, a movie CMAC, and SHA-256 hashes for each 0x40000-byte (256-KiB) video-data chunk. Those structures and crypto inputs must be generated correctly before the writer is enabled.

The `caps:sc` service has ProtoMovieMetaData commands 1106/1107 used for movie metadata generation. These accept a 1280x720 video image buffer and output metadata-related buffers, but the parameter layout is only partially documented. We are investigating this as the preferred system-side implementation route rather than guessing the unknown struct fields or reimplementing private keys.

The metadata-path argument to the writer must therefore point to a complete, correctly formatted Album movie metadata section (thumbnail JPEG plus MakerNote/metadata payload) created by a verified metadata builder. The code deliberately does not accept an arbitrary sidecar file from the UI or fabricate a MakerNote, CMAC, or hash table.

## Important compatibility notes

- The native Album screenshot save service `caps:su` and the `caps:c` movie stream APIs are documented from system version 4.0.0 onward.
- The screenshot capture API accepts a 1280x720 RGBA8 buffer. The NRO decodes JPEG/PNG inputs locally and resizes them before handing them to the service.
- The selected-game attribution for images is requested through `appletSetScreenShotAppletIdentityInfo()`. The Movie file identity is created using the selected Title ID passed to `capscGenerateCurrentAlbumFileId()`. Both paths still require real Switch testing; successful compilation alone does not prove that Album displays the intended title attribution.
- The MP4 preflight is a structural inspection only. It does not decode H.264 samples, validate every sample table entry, extract a thumbnail, or prove that the Album decoder accepts the file.
- The app does not extract or store Nintendo's private screenshot/movie keys. The image system service handles screenshot authentication; the video metadata signer is not implemented yet.
- Switch 2 compatibility is not claimed. Its Album/crypto path needs separate verification.

See [the full design](../../ideas/01-albumsigner.md).
