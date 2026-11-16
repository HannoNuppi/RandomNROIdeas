#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Writes an MP4 plus an already generated Album movie metadata section
 * through libnx's caps:c AlbumMovieWriteStream API.
 *
 * The metadata section must be produced by the future verified metadata
 * builder. This entry point must not be called with an arbitrary sidecar.
 */
bool album_video_import_prepared_on_switch(const char *mp4_path,
                                           const char *metadata_path,
                                           uint64_t application_id,
                                           char *error,
                                           size_t error_size);
