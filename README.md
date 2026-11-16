# RandomNROIdeas

A collection of Nintendo Switch homebrew experiments. The repository currently builds the **AlbumSigner** prototype as a native libnx NRO.

## Build, tests, and releases

Every push to \`main\` runs the Switch compiler and a CI smoke test. The Switch build checks that the ELF is an AArch64 executable, the NRO exists, and the NRO header contains the expected \`NRO0\` magic.

A separate host-side test job exercises the MP4 box parser with valid, unsupported, fragmented, and malformed fixtures. A GitHub prerelease is published only after both jobs succeed. Each release gets a unique \`build-<run>-<attempt>\` tag. Pull requests are build-tested but do not publish releases.

The build fetches a pinned \`stb_image.h\` header at compile time; no image codec packages or package manager are needed. The built NRO has no runtime network dependency.

- [Latest build](https://github.com/HannoNuppi/RandomNROIdeas/actions/workflows/build-nro.yml)
- [All GitHub releases](https://github.com/HannoNuppi/RandomNROIdeas/releases)

## Local build

Install the devkitPro Switch development packages (\`switch-dev\`) in devkitPro MSYS2. The first build downloads the pinned image decoder header:

\`\`\`sh
make
\`\`\`

The output is \`RandomNROIdeas.nro\` in the repository root.

## Project layout

- \`.github/workflows/build-nro.yml\` — compile, smoke-test, run MP4 parser tests, upload artifacts, and publish prereleases
- \`source/main.c\` — application entry point
- \`source/albumsigner.c\` — native AlbumSigner application and UI
- \`source/image_decode.c\` — JPEG/PNG decoding and 1280x720 RGBA conversion
- \`source/video_probe.c\` — bounded ISO BMFF/MP4 container and track inspection
- \`tests/test_video_probe.c\` — host-side MP4 parser regression tests
- \`ideas/01-albumsigner.md\` — AlbumSigner requirements and implementation plan

## AlbumSigner status

**Implemented in the current prototype:**

- Require Application mode.
- Browse the SD card for JPEG, PNG, and MP4 files.
- Enumerate installed games and display their names and Title IDs.
- Read the current local console time.
- Decode JPEG/PNG images locally and fit them into a 1280x720 RGBA buffer.
- Request a real Album screenshot save through the Switch system's \`caps:su\` service, with the selected application identity.
- Inspect MP4 box structure, video codec, resolution, audio codec, duration, and fragmented/fast-start flags without decoding or changing the source file.
- Run host-side regression tests for valid and malformed MP4 box structures.

The original media file remains untouched. The NRO does not extract or embed Nintendo's private Album keys; the system service produces the authenticated Album screenshot entry.

**Not yet implemented or verified:**

- Album video writing: thumbnail generation, movie MakerNote, encrypted metadata, movie CMAC, and the 256-KiB chunk-hash table.
- Video transcoding/remuxing on the Switch. The initial preflight target is non-fragmented H.264/AVC at 1280x720 with AAC audio (or no audio); passing preflight does not mean the file is Album-ready.
- Real-console confirmation that selected-game attribution is accepted on all supported Switch 1 firmware.
- Switch 2 compatibility testing.

A successful CI build and MP4 parser test verify code structure only. They do not replace real-device Album testing or prove that a video is accepted by the Album viewer.
