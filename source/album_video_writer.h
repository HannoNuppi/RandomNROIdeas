#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ALBUM_VIDEO_DATA_CHUNK_SIZE 0x40000u
#define ALBUM_VIDEO_MAX_DATA_SIZE (2ULL * 1024ULL * 1024ULL * 1024ULL)

typedef struct {
    void *context;
    uint32_t (*open)(void *context, uint64_t application_id, uint64_t *out_stream);
    uint32_t (*set_data_size)(void *context, uint64_t stream, uint64_t size);
    uint32_t (*start_data_section)(void *context, uint64_t stream);
    uint32_t (*write_data)(void *context, uint64_t stream,
                          uint64_t offset, const void *buffer, uint64_t size);
    uint32_t (*end_data_section)(void *context, uint64_t stream);
    uint32_t (*start_meta_section)(void *context, uint64_t stream);
    uint32_t (*write_meta)(void *context, uint64_t stream,
                          uint64_t offset, const void *buffer, uint64_t size);
    uint32_t (*end_meta_section)(void *context, uint64_t stream);
    uint32_t (*finish)(void *context, uint64_t stream);
    uint32_t (*commit)(void *context, uint64_t stream);
    uint32_t (*discard)(void *context, uint64_t stream);
} AlbumVideoWriteOps;

/*
 * Writes a prebuilt Album movie package through the platform Album service.
 *
 * mp4_path must be an MP4 that passes the current H.264 1280x720 preflight.
 * metadata_path must contain the complete, correctly formatted Album movie
 * metadata section (thumbnail + maker-note/hash data). This module does not
 * yet generate that metadata section; arbitrary files must not be passed here.
 *
 * Data is streamed in 256-KiB chunks and is never loaded entirely into RAM.
 */
bool album_video_write_prepared(const char *mp4_path,
                                const char *metadata_path,
                                uint64_t application_id,
                                const AlbumVideoWriteOps *ops,
                                char *error,
                                size_t error_size);
