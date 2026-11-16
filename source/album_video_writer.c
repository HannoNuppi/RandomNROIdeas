#include "album_video_writer.h"
#include "video_probe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_error(char *error, size_t error_size, const char *message)
{
    if (error == NULL || error_size == 0)
        return;

    snprintf(error, error_size, "%s", message != NULL ? message : "Unknown video writer error");
}

static void set_result_error(char *error,
                             size_t error_size,
                             const char *operation,
                             uint32_t result)
{
    if (error == NULL || error_size == 0)
        return;

    snprintf(error, error_size, "%s failed (result 0x%08X).",
             operation, (unsigned)(uint32_t)result);
}

static bool ops_are_valid(const AlbumVideoWriteOps *ops)
{
    return ops != NULL &&
           ops->open != NULL &&
           ops->set_data_size != NULL &&
           ops->start_data_section != NULL &&
           ops->write_data != NULL &&
           ops->end_data_section != NULL &&
           ops->start_meta_section != NULL &&
           ops->write_meta != NULL &&
           ops->end_meta_section != NULL &&
           ops->finish != NULL &&
           ops->commit != NULL &&
           ops->discard != NULL;
}

static bool get_file_size(FILE *file, uint64_t *out_size)
{
    long end_position;

    if (file == NULL || out_size == NULL ||
        fseek(file, 0, SEEK_END) != 0 ||
        (end_position = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0)
        return false;

    *out_size = (uint64_t)end_position;
    return true;
}

static bool stream_file(FILE *file,
                        uint64_t expected_size,
                        uint64_t stream,
                        const AlbumVideoWriteOps *ops,
                        bool metadata,
                        char *error,
                        size_t error_size)
{
    uint8_t *buffer;
    uint64_t offset = 0;
    bool succeeded = true;

    buffer = (uint8_t *)malloc(ALBUM_VIDEO_DATA_CHUNK_SIZE);
    if (buffer == NULL) {
        set_error(error, error_size, "Not enough memory for the video stream buffer.");
        return false;
    }

    while (offset < expected_size) {
        const uint64_t remaining = expected_size - offset;
        const size_t request_size = (size_t)(
            remaining < ALBUM_VIDEO_DATA_CHUNK_SIZE
                ? remaining
                : ALBUM_VIDEO_DATA_CHUNK_SIZE
        );
        const size_t bytes_read = fread(buffer, 1, request_size, file);

        if (bytes_read != request_size) {
            set_error(error, error_size, "A media package file ended unexpectedly while streaming.");
            succeeded = false;
            break;
        }

        const uint32_t result = metadata
            ? ops->write_meta(ops->context, stream, offset, buffer, bytes_read)
            : ops->write_data(ops->context, stream, offset, buffer, bytes_read);

        if (result != 0) {
            set_result_error(error, error_size,
                             metadata ? "Album metadata write" : "Album video write",
                             result);
            succeeded = false;
            break;
        }

        offset += bytes_read;
    }

    free(buffer);
    return succeeded;
}

bool album_video_write_prepared(const char *mp4_path,
                                const char *metadata_path,
                                uint64_t application_id,
                                const AlbumVideoWriteOps *ops,
                                char *error,
                                size_t error_size)
{
    FILE *mp4 = NULL;
    FILE *metadata = NULL;
    Mp4Info mp4_info;
    char probe_error[192] = {0};
    uint64_t mp4_size = 0;
    uint64_t metadata_size = 0;
    uint64_t stream = 0;
    bool stream_open = false;
    bool success = false;
    uint32_t result;

    if (mp4_path == NULL || metadata_path == NULL || application_id == 0 ||
        !ops_are_valid(ops)) {
        set_error(error, error_size, "Invalid Album video writer arguments.");
        return false;
    }

    if (!mp4_probe_file(mp4_path, &mp4_info, probe_error, sizeof(probe_error))) {
        set_error(error, error_size, probe_error);
        return false;
    }

    if (!mp4_is_switch_720p_candidate(&mp4_info)) {
        set_error(error, error_size,
                  "The MP4 is outside the current H.264 1280x720 preflight profile.");
        return false;
    }

    mp4 = fopen(mp4_path, "rb");
    metadata = fopen(metadata_path, "rb");
    if (mp4 == NULL || metadata == NULL) {
        set_error(error, error_size, "Could not open both the MP4 and metadata package files.");
        goto cleanup;
    }

    if (!get_file_size(mp4, &mp4_size) ||
        !get_file_size(metadata, &metadata_size)) {
        set_error(error, error_size, "Could not determine the package file sizes.");
        goto cleanup;
    }

    if (mp4_size != mp4_info.file_size) {
        set_error(error, error_size, "The MP4 changed after it was inspected; select it again.");
        goto cleanup;
    }

    if (mp4_size == 0 || mp4_size > ALBUM_VIDEO_MAX_DATA_SIZE) {
        set_error(error, error_size, "The MP4 exceeds the Album service's 2-GiB data limit.");
        goto cleanup;
    }

    if (metadata_size == 0) {
        set_error(error, error_size, "The Album movie thumbnail/metadata package is empty.");
        goto cleanup;
    }

    result = ops->open(ops->context, application_id, &stream);
    if (result != 0) {
        set_result_error(error, error_size, "Opening the Album movie stream", result);
        goto cleanup;
    }
    stream_open = true;

    result = ops->set_data_size(ops->context, stream, mp4_size);
    if (result != 0) {
        set_result_error(error, error_size, "Setting the Album video data size", result);
        goto cleanup;
    }

    result = ops->start_data_section(ops->context, stream);
    if (result != 0) {
        set_result_error(error, error_size, "Starting the Album video data section", result);
        goto cleanup;
    }

    if (!stream_file(mp4, mp4_size, stream, ops, false, error, error_size))
        goto cleanup;

    result = ops->end_data_section(ops->context, stream);
    if (result != 0) {
        set_result_error(error, error_size, "Ending the Album video data section", result);
        goto cleanup;
    }

    result = ops->start_meta_section(ops->context, stream);
    if (result != 0) {
        set_result_error(error, error_size, "Starting the Album metadata section", result);
        goto cleanup;
    }

    if (!stream_file(metadata, metadata_size, stream, ops, true, error, error_size))
        goto cleanup;

    result = ops->end_meta_section(ops->context, stream);
    if (result != 0) {
        set_result_error(error, error_size, "Ending the Album metadata section", result);
        goto cleanup;
    }

    result = ops->finish(ops->context, stream);
    if (result != 0) {
        set_result_error(error, error_size, "Finishing the Album movie stream", result);
        goto cleanup;
    }

    result = ops->commit(ops->context, stream);
    if (result != 0) {
        set_result_error(error, error_size, "Committing the Album movie stream", result);
        goto cleanup;
    }

    stream_open = false;
    success = true;
    set_error(error, error_size, "Album movie stream committed.");
    
cleanup:
    if (stream_open) {
        const uint32_t discard_result = ops->discard(ops->context, stream);
        if (discard_result != 0 && error != NULL && error_size > 0) {
            char previous_error[160];
            snprintf(previous_error, sizeof(previous_error), "%s",
                     error[0] != '\0' ? error : "Video import failed.");
            snprintf(error, error_size,
                     "%s Cleanup also failed (result 0x%08X).",
                     previous_error, (unsigned)discard_result);
        }
    }

    if (mp4 != NULL)
        fclose(mp4);
    if (metadata != NULL)
        fclose(metadata);

    return success;
}
