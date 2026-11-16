# RandomNROIdeas

A collection of Nintendo Switch homebrew experiments. The repository currently builds the **AlbumSigner** prototype as a native libnx NRO.

## Build, tests, and releases

Every push to `main` runs the Switch compiler and a CI smoke test. The test job checks that:

- the ELF is an AArch64 executable;
- the NRO file exists and is non-empty;
- the NRO header contains the expected `NRO0` magic.

Only after that entire job succeeds, GitHub Actions publishes a **GitHub prerelease** with every `.nro` found in the build output. Each release gets a unique `build-<run>-<attempt>` tag, so previous test builds remain available.

Pull requests are build-tested and have an Actions artifact, but they do **not** publish a release. Manually dispatched runs publish only when run against `main`.

- [Latest build](https://github.com/HannoNuppi/RandomNROIdeas/actions/workflows/build-nro.yml)
- [All GitHub releases](https://github.com/HannoNuppi/RandomNROIdeas/releases)

The Actions artifact is retained for 14 days. GitHub prereleases remain available until deleted.

## Local build

Install the devkitPro Switch development packages (`switch-dev`), open the devkitPro MSYS2 shell, then run:

```sh
make
```

The output is `RandomNROIdeas.nro` in the repository root.

## Project layout

- `.github/workflows/build-nro.yml` — compile, smoke-test, upload artifacts, and publish prereleases
- `source/main.c` — application entry point
- `source/albumsigner.c` / `source/albumsigner.h` — AlbumSigner prototype
- `ideas/01-albumsigner.md` — AlbumSigner requirements and implementation plan
- `Makefile` — devkitPro/libnx build configuration

## AlbumSigner prototype status

The current prototype:

1. Requires **Application mode** and exits with a clear message when launched as an applet.
2. Mounts the SD card and browses folders for JPG, JPEG, PNG, MP4, MOV, and M4V files.
3. Enumerates installed applications and displays each game's name and application Title ID.
4. Reads the console's current local date/time and previews the destination date folder under `Nintendo/Album/YYYY/MM/DD/`.

**Important:** this is currently a selection and metadata preview prototype. It does not create or sign Album files yet. The original media remains untouched. Image EXIF/MakerNote authentication and video metadata/hash generation are separate implementation milestones and must be validated on hardware before the UI can claim a file was added successfully.

The NRO is intended to run offline. It uses the Switch's local services and SD storage; no upload endpoint or external signing service is part of the design.
