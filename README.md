# RandomNROIdeas

A collection of Nintendo Switch homebrew experiments. The repository currently builds the **AlbumSigner** prototype as a native libnx NRO.

## Build, tests, and releases

Every push to `main` runs the Switch compiler and a CI smoke test. The test job checks that the ELF is an AArch64 executable, the NRO exists, and the NRO header contains the expected `NRO0` magic.

Only after the entire test job succeeds, GitHub Actions publishes a **GitHub prerelease** with every `.nro` found in the build output. Each release gets a unique `build-<run>-<attempt>` tag. Pull requests are build-tested but do not publish releases.

The build fetches a pinned stb_image header at compile time; no image codec packages or package manager are needed.

- [Latest build](https://github.com/HannoNuppi/RandomNROIdeas/actions/workflows/build-nro.yml)
- [All GitHub releases](https://github.com/HannoNuppi/RandomNROIdeas/releases)

## Local build

Install the devkitPro Switch development packages (`switch-dev`) in devkitPro MSYS2. The first build downloads the pinned image decoder header:

```sh
make
```

The output is `RandomNROIdeas.nro` in the repository root.

## Project layout

- `.github/workflows/build-nro.yml` — compile, smoke-test, upload artifacts, and publish prereleases
- `source/main.c` — application entry point
- `source/albumsigner.c` — native AlbumSigner application and UI
- `source/image_decode.c` — JPEG/PNG decoding and 1280x720 RGBA conversion
- `ideas/01-albumsigner.md` — AlbumSigner requirements and implementation plan

## AlbumSigner status

**Implemented in the current prototype:**
- Require Application mode.
- Browse the SD card for JPEG and PNG images.
- Enumerate installed games and display their names and Title IDs.
- Read the current local console time.
- Convert an image locally to 1280x720 RGBA.
- Request a real Album screenshot save through the Switch system's `caps:su` service, with the selected application identity.

The original image remains untouched. The NRO does not extract or embed Nintendo's private Album keys; the system service produces the authenticated Album entry.

**Not yet implemented or verified:**
- Real-console confirmation that selected-game attribution is accepted on all supported Switch 1 firmware.
- Switch 2 compatibility testing.
- Album video conversion, movie MakerNote generation, and chunk-hash generation.

A successful CI build verifies compilation and NRO structure only. It does not replace real-device Album testing.
