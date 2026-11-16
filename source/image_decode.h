#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ALBUM_IMAGE_WIDTH  1280
#define ALBUM_IMAGE_HEIGHT 720
#define ALBUM_IMAGE_RGBA_SIZE ((size_t)ALBUM_IMAGE_WIDTH * ALBUM_IMAGE_HEIGHT * 4)

bool image_decode_to_rgba(const char *path,
                          uint8_t **out_pixels,
                          unsigned *out_width,
                          unsigned *out_height,
                          char *error,
                          size_t error_size);

bool image_fit_to_album_rgba(const uint8_t *source,
                             unsigned source_width,
                             unsigned source_height,
                             uint8_t **out_pixels,
                             char *error,
                             size_t error_size);
