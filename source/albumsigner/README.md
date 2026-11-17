# AlbumSigner image and video pipeline

The image path is implemented in `../albumsigner.c` and `../image_decode.c`. MP4 inspection is implemented in `../video_probe.c`. The transactional writer is in `../album_video_writer.c` and `../album_video_caps_backend.c`. The experimental `caps:sc` command 1107 adapter is in `../caps_sc_proto_call.c`.

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
10. An experimental low-level wrapper for `caps:sc` cmd 1107 now matches the documented HOS 5-18 and HOS 19+ IPC sizes and buffer directions. It is not connected to the UI because the values inside the opaque input blocks and the output buffer's structure are not yet understood.

## `caps:sc` command 1107 research

The command is named `CreateProtoMovieMetaDataRgbaEx2` and is part of `nn::capsrv::sf::IScreenShotControlService` (`caps:sc`). The public interface describes a 1280x720 RGBA frame input and a metadata output buffer of at least `0x7D000` bytes. It returns three u64 values whose meanings are not described.

The scalar IPC input and fixed-size input buffers are firmware-dependent:

| Firmware | Scalar input size | Fixed-size input buffers |
|---|---:|---|
| HOS 5.0.0-18.x | `0x68` | `0x400`, `0x404` |
| HOS 19.0.0+ | `0x78` | `0x400`, `0x404`, `0x88` |

The documented scalar layout is `0x40 bytes + u64 width + u64 height + 0x18 bytes`; HOS 19+ appends another `0x10` bytes. The wrapper checks that width and height are 1280x720. It does not guess the semantics of the remaining blocks.

The buffer descriptors match the published IPC captures:

- Each fixed-size input uses `SfBufferAttr_In | SfBufferAttr_HipcMapAlias | SfBufferAttr_FixedSize` (descriptor `0x15`).
- The metadata buffer uses `SfBufferAttr_Out | SfBufferAttr_HipcMapAlias` (descriptor `0x06`), minimum `0x7D000`.
- The RGBA frame uses `SfBufferAttr_In | SfBufferAttr_HipcMapAlias | SfBufferAttr_HipcMapTransferAllowsNonSecure` (descriptor `0x45`), minimum `0x384000`.

**Important research limitation:** the Switchbrew prose describes two fixed-size inputs, while the HOS 19+ IPC capture adds a third `0x88` input. The adapter follows the versioned IPC capture: two inputs before HOS 19, three from HOS 19 onward. The third buffer has the same size as libnx's `CapsUserIdList`, but the code does not assume that it is a user-ID list. The first two buffers are also passed as opaque bytes.

HOS 4.0.0-4.1.0 used an older `0x30`-byte scalar signature. This adapter deliberately starts at HOS 5.0.0 and does not guess that older layout.

## What is still missing before video import

A normal MP4 is not a complete Album movie package. The full video path still needs a valid JPEG thumbnail, the 0x498-byte MakerNote, encrypted movie metadata, the movie CMAC, and SHA-256 hashes for each 0x40000-byte (256-KiB) chunk.

The new command wrapper only establishes the IPC call boundary. It does not yet build the opaque input blocks, populate the two/three fixed-size input buffers, interpret the `0x7D000` output buffer, or explain the three returned u64 values. It has not been called on a real Switch. The video writer remains disconnected from the UI until those details are known.

The `caps:sc` service is therefore a promising official system-side route, not yet a working metadata generator in AlbumSigner.

## Dependencies

- libnx from devkitPro
- `stb_image.h` from the pinned `nothings/stb` commit. The Makefile downloads it on the first build and caches it locally; the built NRO has no runtime network dependency.

Run `make` in the devkitPro MSYS2 shell. A first build requires internet access only to fetch this pinned header.

## Important compatibility notes

- The native Album screenshot save service `caps:su` and the `caps:c` movie stream APIs are documented from system version 4.0.0 onward.
- The screenshot capture API accepts a 1280x720 RGBA8 buffer. The NRO decodes JPEG/PNG inputs locally and resizes them before handing them to the service.
- The selected-game attribution for images is requested through `appletSetScreenShotAppletIdentityInfo()`. The Movie file identity is created using the selected Title ID passed to `capscGenerateCurrentAlbumFileId()`. Both paths still require real Switch testing.
- The MP4 preflight is a structural inspection only. It does not decode H.264 samples, validate every sample table entry, extract a thumbnail, or prove that the Album decoder accepts the file.
- The app does not extract or store Nintendo's private screenshot/movie keys. The video metadata command's input/output structures still need research; no video signing is currently performed.
- Switch 2 compatibility is not claimed. Its Album/crypto path needs separate verification.

## References

- [Switchbrew VI services](https://switchbrew.org/wiki/Display_services)
- [Switchbrew 19.0.0 changes](https://switchbrew.org/wiki/19.0.0)
- [HOS 23.0.1 `caps:sc` IPC capture](https://github.com/borntohonk/Switch-Ghidra-Guides/blob/5732ea48463db991eee2db49a70b9006254c934e/ipc_json_dumps/ns1/23.0.1/programs/Display_services.json)
- [HOS 22.5.0 `caps:sc` IPC capture](https://github.com/borntohonk/Switch-Ghidra-Guides/blob/5732ea48463db991eee2db49a70b9006254c934e/ipc_json_dumps/ns1/22.5.0/programs/Display_services.json)

See [the full design](../../ideas/01-albumsigner.md).
