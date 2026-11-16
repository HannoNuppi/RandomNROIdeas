# Idea 01 — AlbumSigner

## Goal

Build a fully native Nintendo Switch homebrew application that prepares user-selected images and videos for the official Album viewer.

The source media stays on the SD card. The NRO performs all processing locally and writes the prepared output to the SD card. It must not upload media to a website or depend on a remote signing API.

The resulting files should be usable by the stock Album viewer on an unmodified target console when that console supports the relevant Album format and accepts the generated authentication data.

## User-facing requirements

- Browse/select an image or video directly from `sdmc:`.
- Choose an installed game from a list populated from the system's installed application records.
- Show the game's readable name and 16-digit application Title ID.
- Allow a manual Title ID fallback when a game is not returned by the installed-title service.
- Allow the capture date/time to be selected, defaulting to the current local time.
- Choose an output mode: a staging folder first, or direct placement in the SD Album folder after an explicit confirmation.
- Never overwrite existing album files. Generate a unique timestamp suffix when necessary.
- Show progress, output size, and actionable errors.
- Work fully offline.

## Media formats and validation

### Images

- Accept common image inputs where a decoder is available (initial target: JPEG and PNG).
- Convert to the dimensions and JPEG profile expected by the Album viewer.
- Write the required EXIF fields, timestamp, and MakerNote.
- Calculate the appropriate Album MAC for the target format generation.
- Verify the output structure before placing it in the final output folder.

The Album validation scheme is version-dependent. Older systems used HMAC-SHA256; from system version 3.0.0 the documented path uses SHA-256 followed by SPL AES-CMAC. The implementation must detect the available path and must not silently write a file with a guessed or invalid MAC.

### Videos

- Accept common input containers only when a decoder/demuxer is available.
- Produce an MP4 stream compatible with the Switch Album playback path; the current Flexlion page describes 720p H.264 output.
- Generate the required JPEG preview/thumbnail and video MakerNote.
- Generate the encrypted movie metadata and per-chunk SHA-256 hash table. The documented chunk size is `0x40000` (256 KiB).
- Stream media data and hash chunks instead of loading an entire large video into RAM.
- Validate MP4 structure, metadata lengths, hash-table coverage, and output sizes before writing the final file.

Video support is a separate milestone from image support. A regular MP4 with a renamed extension is not a valid Album video.

## Title ID handling

The user should select an installed title by its readable game name. The NRO can enumerate installed application records through libnx's `nsListApplicationRecord` and retrieve control data through `nsGetApplicationControlData`.

The user-facing 64-bit application Title ID must not be confused with the Album filename's title-specific identifier. The exact mapping between those values must be implemented from verified Album behavior, not guessed or substituted directly.

## Size and performance

- There should be no arbitrary 30 MB or 50 MB upload limit because media is read locally from SD.
- Process files incrementally so memory use does not scale with the full input file size.
- Available SD space and filesystem limits still apply. In particular, a FAT32 volume cannot store a single file of 4 GiB or larger.
- Decoding/re-encoding long video on Switch 1 may be slow and memory-intensive. Prefer streaming and avoid unnecessary transcoding when the source already matches the target profile.
- Do not claim unlimited file sizes: the actual maximum is constrained by filesystem, free space, decoder limits, and Album/system limits.

## Compatibility plan

### Switch 1

Switch 1 is the first development and validation target because this repository builds a libnx NRO for devkitA64. The implementation should test both the legacy image-MAC path and the modern SPL CMAC path where appropriate.

### Switch 2

Switch 2 requires a separate compatibility check. Its SPL services were overhauled, and the key-derivation interfaces exposed on Switch 1 are not exposed in the same way. Nintendo also documents importing content from a Switch console's microSD card into Switch 2 system memory; this is a useful official transfer path, but it does not by itself prove that files created by this tool will pass Switch 2 validation.

Do not advertise Switch 2 support until the output has been tested through the actual Switch 2 Album import/view path on current retail firmware.

### Unmodified target consoles

The NRO itself runs on the development/source console. The generated files can then be copied to another console's supported Album storage/import path. Acceptance depends on the target console's Album parser, media profile, and MAC/hash validation. A selected Title ID alone is not a signature.

## Implementation milestones

1. **Format research and test fixtures**
   - Document the image MakerNote layout and exact Title ID-to-Album-ID mapping.
   - Collect known-good stock Album files as local test fixtures.
   - Establish a test harness that compares generated files with stock files without embedding any private console material.

2. **Image pipeline**
   - SD file browser and installed-title picker.
   - JPEG/PNG decode, 1280x720 output, EXIF/MakerNote writer.
   - Version-aware MAC backend and output validation.
   - First end-to-end Switch 1 test.

3. **Video pipeline**
   - MP4 demux/decode/encode strategy.
   - Thumbnail, MakerNote encryption, MAC, and chunk-hash table.
   - Large-file streaming and recoverable output writes.
   - Playback tests at multiple sizes and durations.

4. **Switch 2 compatibility**
   - Verify the official Switch 1 microSD import path.
   - Test generated images and videos separately.
   - Add a Switch 2 profile only after a passing real-device test.

## Security and data safety

- Do not commit hardcoded console secrets, console dumps, or private key material.
- Prefer the platform's supported crypto service when available.
- If the required crypto operation is unavailable, stop with a clear unsupported-firmware message rather than outputting invalid files.
- Keep originals untouched.
- Write to a temporary file first, flush it, validate it, and only then move it into the output directory.

## Known references

- Flexlion's current public page: `https://flexlion.github.io/albumsigner.html` (currently documents a 50 MB web upload cap, JPEG image processing, and 720p H.264 video output).
- Switchbrew Capture services: `https://switchbrew.org/wiki/Capture_services`.
- Switchbrew SD filesystem: `https://switchbrew.org/wiki/SD_Filesystem`.
- libnx `ns.h` API reference: `https://switchbrew.github.io/libnx/ns_8h.html`.
- libnx `spl.h` API reference: `https://switchbrew.github.io/libnx/spl_8h.html`.
- Nintendo Switch 2 Album management/import support: `https://www.nintendo.com/de-de/Support/Fehlersuche/Screenshots-und-aufgenommene-Videos-auf-Nintendo-Switch-2-verwalten-2881207.html`.
