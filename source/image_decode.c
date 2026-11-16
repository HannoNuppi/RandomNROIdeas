#include "image_decode.h"

#include <stdio.h>
#include <jpeglib.h>
#include <png.h>

#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define MAX_DECODED_PIXELS 30000000ULL

typedef struct {
    struct jpeg_error_mgr base;
    jmp_buf jump;
    char message[JMSG_LENGTH_MAX];
} JpegError;

static void set_error(char *error, size_t error_size, const char *message)
{
    if (error == NULL || error_size == 0)
        return;

    snprintf(error, error_size, "%s", message != NULL ? message : "Unknown image error");
}

static void jpeg_error_exit(j_common_ptr common)
{
    JpegError *error = (JpegError *)common->err;
    (*common->err->format_message)(common, error->message);
    longjmp(error->jump, 1);
}

static bool dimensions_are_safe(unsigned width, unsigned height)
{
    if (width == 0 || height == 0)
        return false;

    if (width > 20000 || height > 20000)
        return false;

    return (unsigned long long)width * (unsigned long long)height <= MAX_DECODED_PIXELS;
}

static bool decode_jpeg(const char *path,
                        uint8_t **out_pixels,
                        unsigned *out_width,
                        unsigned *out_height,
                        char *error,
                        size_t error_size)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        set_error(error, error_size, "Could not open JPEG file.");
        return false;
    }

    struct jpeg_decompress_struct decoder;
    JpegError jpeg_error;
    memset(&decoder, 0, sizeof(decoder));
    memset(&jpeg_error, 0, sizeof(jpeg_error));

    decoder.err = jpeg_std_error(&jpeg_error.base);
    jpeg_error.base.error_exit = jpeg_error_exit;

    volatile bool decoder_created = false;
    volatile uint8_t *pixels = NULL;
    volatile uint8_t *row = NULL;

    if (setjmp(jpeg_error.jump)) {
        if (decoder_created)
            jpeg_destroy_decompress(&decoder);
        if (pixels != NULL)
            free((void *)pixels);
        if (row != NULL)
            free((void *)row);
        fclose(file);
        set_error(error, error_size, jpeg_error.message[0] ? jpeg_error.message : "Invalid JPEG data.");
        return false;
    }

    jpeg_create_decompress(&decoder);
    decoder_created = true;
    jpeg_stdio_src(&decoder, file);

    if (jpeg_read_header(&decoder, TRUE) != JPEG_HEADER_OK) {
        jpeg_destroy_decompress(&decoder);
        decoder_created = false;
        fclose(file);
        set_error(error, error_size, "JPEG header is invalid.");
        return false;
    }

    decoder.out_color_space = JCS_RGB;
    jpeg_start_decompress(&decoder);

    const unsigned width = decoder.output_width;
    const unsigned height = decoder.output_height;
    if (!dimensions_are_safe(width, height) || decoder.output_components != 3) {
        jpeg_destroy_decompress(&decoder);
        fclose(file);
        set_error(error, error_size, "JPEG is too large or has an unsupported pixel format (limit: 30 megapixels).");
        return false;
    }

    const size_t pixel_count = (size_t)width * height;
    pixels = malloc(pixel_count * 4);
    row = malloc((size_t)width * 3);
    if (pixels == NULL || row == NULL) {
        free((void *)pixels);
        free((void *)row);
        pixels = NULL;
        row = NULL;
        jpeg_destroy_decompress(&decoder);
        decoder_created = false;
        fclose(file);
        set_error(error, error_size, "Not enough memory to decode JPEG.");
        return false;
    }

    while (decoder.output_scanline < decoder.output_height) {
        JSAMPROW rows[1];
        rows[0] = (JSAMPROW)row;

        if (jpeg_read_scanlines(&decoder, rows, 1) != 1) {
            free((void *)pixels);
            free((void *)row);
            pixels = NULL;
            row = NULL;
            jpeg_destroy_decompress(&decoder);
            decoder_created = false;
            fclose(file);
            set_error(error, error_size, "JPEG decoder stopped before the image was complete.");
            return false;
        }

        const size_t y = decoder.output_scanline - 1;
        const uint8_t *rgb = (const uint8_t *)row;
        uint8_t *rgba = (uint8_t *)pixels + y * width * 4;

        for (unsigned x = 0; x < width; x++) {
            rgba[x * 4 + 0] = rgb[x * 3 + 0];
            rgba[x * 4 + 1] = rgb[x * 3 + 1];
            rgba[x * 4 + 2] = rgb[x * 3 + 2];
            rgba[x * 4 + 3] = 255;
        }
    }

    jpeg_finish_decompress(&decoder);
    jpeg_destroy_decompress(&decoder);
    decoder_created = false;

    free((void *)row);
    row = NULL;
    fclose(file);

    *out_pixels = (uint8_t *)pixels;
    *out_width = width;
    *out_height = height;
    return true;
}

static bool decode_png(const char *path,
                       uint8_t **out_pixels,
                       unsigned *out_width,
                       unsigned *out_height,
                       char *error,
                       size_t error_size)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        set_error(error, error_size, "Could not open PNG file.");
        return false;
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png == NULL) {
        fclose(file);
        set_error(error, error_size, "Could not initialize PNG decoder.");
        return false;
    }

    png_infop info = png_create_info_struct(png);
    if (info == NULL) {
        png_destroy_read_struct(&png, NULL, NULL);
        fclose(file);
        set_error(error, error_size, "Could not allocate PNG metadata.");
        return false;
    }

    uint8_t * volatile pixels = NULL;
    png_bytep * volatile rows = NULL;

    if (setjmp(png_jmpbuf(png))) {
        free((void *)rows);
        free((void *)pixels);
        png_destroy_read_struct(&png, &info, NULL);
        fclose(file);
        set_error(error, error_size, "PNG decoding failed or the file is invalid.");
        return false;
    }

    png_init_io(png, file);
    png_read_info(png, info);

    png_uint_32 width = 0;
    png_uint_32 height = 0;
    int bit_depth = 0;
    int color_type = 0;
    int interlace_type = 0;
    int compression_type = 0;
    int filter_method = 0;

    png_get_IHDR(png, info, &width, &height, &bit_depth, &color_type,
                 &interlace_type, &compression_type, &filter_method);

    if (width > 20000 || height > 20000 ||
        (unsigned long long)width * (unsigned long long)height > MAX_DECODED_PIXELS) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(file);
        set_error(error, error_size, "PNG is too large (limit: 30 megapixels).");
        return false;
    }

    if (bit_depth == 16)
        png_set_strip_16(png);
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);

    const bool has_alpha = (color_type & PNG_COLOR_MASK_ALPHA) != 0 ||
                           png_get_valid(png, info, PNG_INFO_tRNS) != 0;
    if (!has_alpha)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

    png_set_interlace_handling(png);
    png_read_update_info(png, info);

    const size_t expected_row_bytes = (size_t)width * 4;
    const png_size_t actual_row_bytes = png_get_rowbytes(png, info);
    if (actual_row_bytes != expected_row_bytes)
        png_error(png, "Unexpected PNG row size.");

    pixels = malloc(expected_row_bytes * height);
    rows = malloc(sizeof(*rows) * height);
    if (pixels == NULL || rows == NULL)
        png_error(png, "Not enough memory for PNG pixels.");

    for (png_uint_32 y = 0; y < height; y++)
        rows[y] = pixels + (size_t)y * expected_row_bytes;

    png_read_image(png, rows);
    png_read_end(png, info);

    png_destroy_read_struct(&png, &info, NULL);
    free((void *)rows);
    fclose(file);

    *out_pixels = (uint8_t *)pixels;
    *out_width = (unsigned)width;
    *out_height = (unsigned)height;
    return true;
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

    const char *dot = strrchr(path, '.');
    if (dot == NULL) {
        set_error(error, error_size, "File extension is missing.");
        return false;
    }

    if (strcasecmp(dot, ".jpg") == 0 || strcasecmp(dot, ".jpeg") == 0)
        return decode_jpeg(path, out_pixels, out_width, out_height, error, error_size);

    if (strcasecmp(dot, ".png") == 0)
        return decode_png(path, out_pixels, out_width, out_height, error, error_size);

    set_error(error, error_size, "Only JPEG and PNG images are supported by this build.");
    return false;
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
