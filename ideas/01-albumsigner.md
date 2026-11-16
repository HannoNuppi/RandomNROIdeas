# Idea 01 — AlbumSigner

## Goal

Build a fully native, offline Nintendo Switch homebrew application that places selected images and videos into the Switch Album as entries attributed to a user-selected installed game, with the current console date/time.

The NRO reads media directly from the SD card, prepares the Album-specific metadata locally, and writes the resulting file into the appropriate Album directory. There is no website, upload, account, or remote signing API.

The intended result is an Album entry that the stock Album viewer can display and play on a compatible target console, including an unmodified console. Compatibility must be verified for each target platform/format generation before it is advertised.

## Runtime requirements

- The NRO is allowed to require **Application mode**. Applet mode is explicitly not a target.
- On startup, check `appletGetAppletType()`. If the NRO is not running as `AppletType_Application`, show a clear message and exit before processing files.
- The NRO cannot grant itself Application mode. The user must launch it through a homebrew setup that provides the Application-mode environment (for example, a title-override launch).
- Application mode provides a larger application memory budget, but it does not make RAM or file size unlimited.
- The program should use the normal libnx SD filesystem APIs and the available system services directly.

## Intended workflow

1. Start AlbumSigner in Application mode.
2. Browse the SD card and select one image or video.
3. Choose an installed game from a list populated by the Switch's installed application records. Show its icon/name and 16-digit application Title ID.
4. The capture timestamp defaults automatically to the **current local time from the console's clock**. No old timestamp should be copied from the source media.
5. Select **Add to Album**.
6. The NRO prepares a separate Album-ready output file, leaving the original input untouched.
7. Write the output directly into the SD Album directory under the current date:
   `Nintendo/Album/YYYY/MM/DD/`
8. Use the Album filename convention and choose a free per-second index. Never overwrite an existing entry.
9. Display the created path, media type, selected game, timestamp, and any compatibility warning.

The output should look like a capture made just now by the selected game. This is an Album attribution/metadata entry: it does not modify the selected game's save data or claim that the game process actually generated the media.

## Title selection and Album attribution

- Enumerate installed application records through libnx NS services, including `nsListApplicationRecord`.
- Read the localized application name and icon/control metadata using `nsGetApplicationControlData`.
- Use the selected installed game's actual application Title ID as the source of the attribution.
- Keep the 64-bit application Title ID distinct from any Album-specific 128-bit identifier or filename field. Implement the mapping only after confirming it against known-good captures and the target Album generation.
- A manual raw-ID field may be added later for debugging, but the normal UI should use the installed-game list.

libnx API reference: `https://switchbrew.github.io/libnx/ns_8h.html`.

## Offline and storage requirements

- No network access is needed or permitted by the normal processing path.
- All decoding, metadata generation, authentication operations, and file writing happen on the Switch.
- There is no arbitrary 30 MB or 50 MB web-upload limit.
- Keep memory use bounded: stream large media and hash-table generation in chunks rather than reading the entire file into RAM.
- Check free space before writing. FAT32 cannot store an individual file of 4 GiB or larger; other filesystem and Album limits still apply.
- Write to a temporary file first, flush and validate it, then rename it into the final Album directory.
- Leave the original image/video unchanged. Never overwrite existing Album entries.

## Media format and validation

### Images

- Implemented first pass: decode JPEG and PNG locally with devkitPro's JPEG/PNG libraries.
- Fit the decoded pixels to an opaque 1280x720 RGBA buffer, preserving the aspect ratio.
- Use libnx `capssuSaveScreenShot()` (the Switch `caps:su` system service) to save the RGBA buffer as a system-generated Album screenshot.
- Request the selected installed game's screenshot identity with `appletSetScreenShotAppletIdentityInfo()`.
- Let the Switch system service create its own EXIF/MakerNote and Album authentication. The NRO does not embed, extract, or store Nintendo's private keys.

The native `caps:su` screenshot-saving API is documented from system version 4.0.0 onward and expects a 1280x720 RGBA8 image buffer. Selected-game attribution still requires validation on real hardware; a successful service call and CI build do not guarantee that every system version displays the requested attribution.

The earlier raw-file signing path (manual EXIF/MakerNote/MAC writing) is therefore not the first implementation path. Prefer the official system save service where available; only investigate direct file generation for platform versions where no supported API exists.

### Videos

- Initial input target: MP4; additional containers only when a reliable local demux/decode path is available.
- Produce a Switch Album-compatible MP4 stream. The existing Flexlion page describes 720p H.264 output, which is a useful initial compatibility target, not proof that every codec/profile is accepted.
- Generate the JPEG preview/thumbnail and required video MakerNote.
- Generate encrypted movie metadata and the per-chunk SHA-256 hash table. The documented chunk size is `0x40000` (256 KiB).
- Stream video data and hashes in bounded chunks.
- Validate the MP4 structure, metadata lengths, thumbnail, hash-table coverage, and output size.

A normal MP4 with an EXIF block or renamed extension is not automatically a valid Album video. Video support is a separate milestone after image support.

## Cryptographic backends

- Use the console's supported system crypto service for the target Album generation where the required operation is available.
- Never commit console secrets, private key material, console dumps, or reusable key dumps to this repository.
- Switch 1 needs a version-aware path for the legacy HMAC generation and the newer SPL CMAC path.
- Switch 2 uses a substantially different SPL service interface. Its public service documentation describes a separate `spl:da` interface, including commands associated with loading the screenshot/movie CMAC keys. Treat it as a distinct backend rather than assuming Switch 1 calls work unchanged.
- If a required crypto operation is unavailable, stop with a clear unsupported-platform/firmware message. Do not create a file with a fabricated MAC.

Research references:
- Capture services: `https://switchbrew.org/wiki/Capture_services`
- SPL services: `https://switchbrew.org/wiki/SPL_services`
- libnx SPL API: `https://switchbrew.github.io/libnx/spl_8h.html`

## Compatibility plan

### Switch 1

- First NRO development target, using the repository's devkitA64/libnx toolchain.
- Require Application mode.
- Test image generation first, then video.
- Test the relevant legacy and SPL-CMAC validation generations using known-good, locally captured fixtures.

### Switch 2

- Treat the Switch 2 Album format and cryptographic backend separately.
- The NRO is not expected to run on an unmodified target console. It runs offline on the homebrew/development console and prepares files for transfer/import to the target.
- Test generated images and videos with the actual Switch 2 Album import/view path on current retail firmware.
- Do not advertise Switch 2 support until this real-device test passes. An official SD-card import function is only a transfer path; it does not prove that an arbitrary generated file passes Album validation.

### Unmodified target consoles

- No homebrew needs to run on the target console. The target only receives and opens the prepared media through its supported Album storage/import path.
- Whether the target displays the entry depends on its Album parser, accepted media profile, identifier mapping, and MAC/hash validation.
- “Works on all Switch systems” is a test target, not a claim to make before compatibility has been demonstrated.

## Implementation milestones

1. **Application-mode shell and title browser**
   - Detect and require Application mode.
   - Mount/access SD storage and browse files.
   - Enumerate installed titles and display game name/icon/Title ID.
   - Read the console's current local timestamp.
   - Show a dry-run destination path without writing media.

2. **Image pipeline**
   - JPEG/PNG decode and output profile.
   - EXIF/MakerNote generation.
   - Version-aware system crypto backend.
   - Collision-safe writing into the date-based Album directory.
   - First end-to-end Switch 1 test, then target-console playback tests.

3. **Video pipeline**
   - Local MP4 input validation and conversion strategy.
   - Thumbnail, encrypted MakerNote, MAC, and chunk-hash table.
   - Streaming processing for large files.
   - Playback tests across durations and file sizes.

4. **Switch 2 compatibility**
   - Research and implement its separate crypto backend if available to the source environment.
   - Verify the official import/view path with generated image and video fixtures.
   - Add a Switch 2 profile only after successful retail-device tests.

## References

- Flexlion Album Signer: `https://flexlion.github.io/albumsigner.html` (the current page documents a 50 MB web upload cap, JPEG image processing, and 720p H.264 video output).
- Switchbrew Capture services: `https://switchbrew.org/wiki/Capture_services`.
- Switchbrew SPL services: `https://switchbrew.org/wiki/SPL_services`.
- Switchbrew SD filesystem: `https://switchbrew.org/wiki/SD_Filesystem`.
- libnx NS API: `https://switchbrew.github.io/libnx/ns_8h.html`.
- libnx SPL API: `https://switchbrew.github.io/libnx/spl_8h.html`.
- Nintendo Switch 2 Album management/import support: `https://www.nintendo.com/de-de/Support/Fehlersuche/Screenshots-und-aufgenommene-Videos-auf-Nintendo-Switch-2-verwalten-2881207.html`.
