# RandomNROIdeas

A collection of Nintendo Switch homebrew experiments. The repository currently builds the **AlbumSigner** prototype as a native libnx NRO.

## Build, tests, and releases

Every push to `main` runs the Switch compiler and a CI smoke test. The Switch build checks that the ELF is an AArch64 executable, the NRO exists, and the NRO header contains the expected `NRO0` magic.

A separate host-side test job exercises the MP4 box parser and the streaming Album movie writer with valid, unsupported, fragmented, malformed, successful, and aborted-write fixtures. A GitHub prerelease is published only after both host tests and the Switch NRO smoke test succeed. Each release gets a unique `build-<run>-<attempt>` tag. Pull requests are build-tested but do not publish releases.

The build fetches a pinned `stb_image.h` header at compile time; no image codec packages or package manager are needed. The built NRO has no runtime network dependency.

- [Latest build](https://github.com/HannoNuppi/RandomNROIdeas/actions/workflows/build-nro.yml)
- [All GitHub releases](https://github.com/HannoNuppi/RandomNROIdeas/releases)

## Local build

Install the devkitPro Switch development packages (`switch-dev`) in devkitPro MSYS2. The first build downloads the pinned image decoder header:

```sh
make
```

The output is `RandomNROIdeas.nro` in the repository root.

## Project layout

- `.github/workflows/build-nro.yml` — compile, smoke-test, run MP4 parser tests, upload artifacts, and publish prereleases
- `source/main.c` — application entry point
- `source/albumsigner.c` — native AlbumSigner application and UI
- `source/image_decode.c` — JPEG/PNG decoding and 1280x720 RGBA conversion
- `source/video_probe.c` — bounded ISO BMFF/MP4 container and track inspection
- `source/album_video_writer.c` — 256-KiB streamed data/metadata writer with transaction cleanup
- `source/album_video_caps_backend.c` — libnx `caps:c` AlbumMovieWriteStream adapter
- `tests/test_video_probe.c` and `tests/test_album_video_writer.c` — host-side MP4 and stream-writer regression tests
- `ideas/01-albumsigner.md` — AlbumSigner requirements and implementation plan

## AlbumSigner status

**Implemented in the current prototype:**

- Require Application mode.
- Browse the SD card for JPEG, PNG, and MP4 files.
- Enumerate installed games and display their names and Title IDs.
- Read the current local console time.
- Decode JPEG/PNG images locally and fit them into a 1280x720 RGBA buffer.
- Request a real Album screenshot save through the Switch system's `caps:su` service, with the selected application identity.
- Inspect MP4 box structure, video codec, resolution, audio codec, duration, and fragmented/fast-start flags without decoding or changing the source file.
- Run host-side regression tests for MP4 parsing and the streamed writer's successful and abort paths.
- Build a libnx `caps:c` backend for the system's `AlbumMovieWriteStream`, which can stream video data and a prebuilt metadata section into the normal Album. This backend is deliberately not called by the UI yet.

The original media file remains untouched. The NRO does not extract or embed Nintendo's private Album keys. Images are authenticated by `caps:su`; the video backend delegates storage/commit to `caps:c`, but its required movie metadata is not generated yet.

**Not yet implemented or verified:**

- Album video metadata generation: JPEG thumbnail, 0x498-byte movie MakerNote, encrypted metadata, movie CMAC, and SHA-256 chunk-hash table. The generic `caps:c` stream writer is implemented, but no valid metadata package is generated yet and the UI does not call it. The related `caps:sc` ProtoMovieMetaData commands are being researched as a possible system-side generator.
- Video transcoding/remuxing on the Switch. The initial preflight target is non-fragmented H.264/AVC at 1280x720 with AAC audio (or no audio); passing preflight does not mean the file is Album-ready.
- Real-console confirmation that selected-game attribution is accepted on all supported Switch 1 firmware.
- Switch 2 compatibility testing.

A successful CI build and MP4 parser test verify code structure only. They do not replace real-device Album testing or prove that a video is accepted by the Album viewer.
