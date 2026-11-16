#include "image_decode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define MAX_DECODED_PIXELS 30000000ULL

static void set_error(char *error, size_t error_size, const char *message)
{
    if (error == NULL || error_size == 0)
        return;

    snprintf(error, error_size, "%s", message != NULL ? message : "Unknown image error");
}

static bool dimensions_are_safe(unsigned width, unsigned height)
{
    if (width == 0 || height == 0)
        return false;

    if (width > 20000 || height > 20000)
        return false;

    return (unsigned long long)width * (unsigned long long)height <= MAX_DECODED_PIXELS;
}

bool image_decode_to_rgba(const char *path,
                          uint8_t **out_pixels,
                          unsigned *out_width,
                          unsigned *out_height,
                          char *error,
                          size_t error_size)
{
    if (path == NULL || out_pixels == NULL || out_width == NULL || out_height == NULL) {
        set_error(error, error_size, "Invalid decoder arguments.");
        return false;
    }

    *out_pixels = NULL;
    *out_width = 0;
    *out_height = 0;

    int width = 0;
    int height = 0;
    int source_channels = 0;

    if (!stbi_info(path, &width, &height, &source_channels)) {
        const char *reason = stbi_failure_reason();
        set_error(error, error_size, reason != NULL ? reason : "The file is not a supported JPEG or PNG image.");
        return false;
    }

    if (width <= 0 || height <= 0 ||
        !dimensions_are_safe((unsigned)width, (unsigned)height)) {
        set_error(error, error_size, "Image is too large (limit: 30 megapixels).");
        return false;
    }

    stbi_uc *pixels = stbi_load(path, &width, &height, &source_channels, STBI_rgb_alpha);
    if (pixels == NULL) {
        const char *reason = stbi_failure_reason();
        set_error(error, error_size, reason != NULL ? reason : "Image decoding failed.");
        return false;
    }

    if (width <= 0 || height <= 0 ||
        !dimensions_are_safe((unsigned)width, (unsigned)height)) {
        stbi_image_free(pixels);
        set_error(error, error_size, "Decoded image dimensions are outside the supported range.");
        return false;
    }

    *out_pixels = pixels;
    *out_width = (unsigned)width;
    *out_height = (unsigned)height;
    return true;
}

static double clamp_double(double value, double minimum, double maximum)
{
    if (value < minimum)
        return minimum;
    if (value > maximum)
        return maximum;
    return value;
}

static uint8_t interpolate_channel(const uint8_t *source,
                                   unsigned width,
                                   int x0, int x1,
                                   int y0, int y1,
                                   double fx, double fy,
                                   unsigned channel)
{
    const size_t p00 = ((size_t)y0 * width + (unsigned)x0) * 4 + channel;
    const size_t p10 = ((size_t)y0 * width + (unsigned)x1) * 4 + channel;
    const size_t p01 = ((size_t)y1 * width + (unsigned)x0) * 4 + channel;
    const size_t p11 = ((size_t)y1 * width + (unsigned)x1) * 4 + channel;

    const double top = source[p00] + (source[p10] - source[p00]) * fx;
    const double bottom = source[p01] + (source[p11] - source[p01]) * fx;
    const double value = top + (bottom - top) * fy;

    if (value < 0.0)
        return 0;
    if (value > 255.0)
        return 255;
    return (uint8_t)(value + 0.5);
}

bool image_fit_to_album_rgba(const uint8_t *source,
                             unsigned source_width,
                             unsigned source_height,
                             uint8_t **out_pixels,
                             char *error,
                             size_t error_size)
{
    if (source == NULL || out_pixels == NULL ||
        source_width == 0 || source_height == 0 ||
        !dimensions_are_safe(source_width, source_height)) {
        set_error(error, error_size, "Invalid source image dimensions.");
        return false;
    }

    uint8_t *output = malloc(ALBUM_IMAGE_RGBA_SIZE);
    if (output == NULL) {
        set_error(error, error_size, "Not enough memory for the Album image.");
        return false;
    }

    for (size_t i = 0; i < ALBUM_IMAGE_WIDTH * ALBUM_IMAGE_HEIGHT; i++) {
        output[i * 4 + 0] = 0;
        output[i * 4 + 1] = 0;
        output[i * 4 + 2] = 0;
        output[i * 4 + 3] = 255;
    }

    const double scale_x = (double)ALBUM_IMAGE_WIDTH / (double)source_width;
    const double scale_y = (double)ALBUM_IMAGE_HEIGHT / (double)source_height;
    const double scale = scale_x < scale_y ? scale_x : scale_y;

    unsigned scaled_width = (unsigned)((double)source_width * scale + 0.5);
    unsigned scaled_height = (unsigned)((double)source_height * scale + 0.5);
    if (scaled_width == 0) scaled_width = 1;
    if (scaled_height == 0) scaled_height = 1;
    if (scaled_width > ALBUM_IMAGE_WIDTH) scaled_width = ALBUM_IMAGE_WIDTH;
    if (scaled_height > ALBUM_IMAGE_HEIGHT) scaled_height = ALBUM_IMAGE_HEIGHT;

    const unsigned offset_x = (ALBUM_IMAGE_WIDTH - scaled_width) / 2;
    const unsigned offset_y = (ALBUM_IMAGE_HEIGHT - scaled_height) / 2;

    for (unsigned dy = 0; dy < scaled_height; dy++) {
        const double source_y = clamp_double(((double)dy + 0.5) / scale - 0.5,
                                             0.0, (double)source_height - 1.0);
        const int y0 = (int)source_y;
        const int y1 = y0 + 1 < (int)source_height ? y0 + 1 : y0;
        const double fy = source_y - y0;

        for (unsigned dx = 0; dx < scaled_width; dx++) {
            const double source_x = clamp_double(((double)dx + 0.5) / scale - 0.5,
                                                 0.0, (double)source_width - 1.0);
            const int x0 = (int)source_x;
            const int x1 = x0 + 1 < (int)source_width ? x0 + 1 : x0;
            const double fx = source_x - x0;

            const size_t target = ((size_t)(offset_y + dy) * ALBUM_IMAGE_WIDTH +
                                   (offset_x + dx)) * 4;

            const uint8_t alpha = interpolate_channel(source, source_width,
                                                      x0, x1, y0, y1, fx, fy, 3);
            for (unsigned channel = 0; channel < 3; channel++) {
                const uint8_t value = interpolate_channel(source, source_width,
                                                          x0, x1, y0, y1, fx, fy, channel);
                output[target + channel] = (uint8_t)(((unsigned)value * alpha + 127) / 255);
            }
            output[target + 3] = 255;
        }
    }

    *out_pixels = output;
    return true;
}
