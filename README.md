# RandomNROIdeas

A small collection of Nintendo Switch homebrew experiments and ideas.

The repository starts with a minimal **libnx** application and a GitHub Actions
workflow that compiles it into a `.nro` file on every push to `main`, on pull
requests, and when manually started from the Actions tab.

## Build output

The workflow uploads **`RandomNROIdeas.nro`** as an Actions artifact. Open the
successful run under **Actions → Build Switch NRO** and download its artifact.
The artifact is a build output, not an automatically published GitHub Release.

## Local build

Install the devkitPro Switch development packages (`switch-dev`), open the
devkitPro MSYS2 shell, then run:

```sh
make
```

The output is `RandomNROIdeas.nro` in the repository root.

## Project layout

- `.github/workflows/build-nro.yml` — automated NRO build
- `source/main.c` — starter libnx application
- `Makefile` — devkitPro/libnx build configuration
- `ideas/` — notes and specifications for future NRO ideas

The starter application displays a simple console message and returns to
Homebrew Menu when **+** is pressed. Replace or extend it as the first ideas
take shape.

## First planned feature: AlbumSigner

The initial design for a native, offline AlbumSigner is in [ideas/01-albumsigner.md](ideas/01-albumsigner.md).
It will prepare images and videos locally on the SD card, with installed-game selection,
firmware-aware Album metadata, and a streaming video pipeline. The actual signer is not
marked complete until its generated files pass real-device validation.
