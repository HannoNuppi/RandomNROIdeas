#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* caps:sc CreateProtoMovieMetaDataRgbaEx2 contract (cmd 1107). */
#define CAPS_SC_PROTO_RGBA_CMD 1107u
#define CAPS_SC_PROTO_INPUT_SIZE_HOS_5_TO_18 0x68u
#define CAPS_SC_PROTO_INPUT_SIZE_HOS_19_PLUS 0x78u

#define CAPS_SC_PROTO_INPUT_BUFFER0_SIZE 0x400u
#define CAPS_SC_PROTO_INPUT_BUFFER1_SIZE 0x404u
#define CAPS_SC_PROTO_INPUT_BUFFER2_SIZE 0x88u /* Added on HOS 19.0.0+. */
#define CAPS_SC_PROTO_OUTPUT_BUFFER_MIN_SIZE 0x7D000u
#define CAPS_SC_PROTO_RGBA_FRAME_MIN_SIZE 0x384000u

/*
 * The documented scalar input layout is:
 *   0x40-byte opaque block + u64 width + u64 height + 0x18-byte opaque block
 * and, on HOS 19.0.0+, an additional 0x10-byte extension.
 *
 * The opaque block contents are intentionally not guessed here.
 */
typedef struct {
    uint8_t header_0x40[0x40];
    uint64_t width;
    uint64_t height;
    uint8_t identity_0x18[0x18];
} CapsScProtoMovieMetaInputV5;

typedef struct {
    CapsScProtoMovieMetaInputV5 base;
    uint8_t extension_hos19_0x10[0x10];
} CapsScProtoMovieMetaInputV19;

typedef struct {
    /* 0x68 bytes on HOS 5-18, 0x78 bytes on HOS 19+. */
    const void *input_data;
    size_t input_data_size;

    /*
     * These input buffers are fixed-size IPC buffers. Their exact semantics
     * are not fully described by the public reference, so they remain opaque.
     * Buffers 0 and 1 exist on HOS 5+; buffer 2 is added on HOS 19+.
     */
    const void *input_buffer0;
    size_t input_buffer0_size;
    const void *input_buffer1;
    size_t input_buffer1_size;
    const void *input_buffer2;
    size_t input_buffer2_size;

    /* Type-0x6 output buffer; minimum capacity 0x7D000 bytes. */
    void *metadata_output;
    size_t metadata_output_size;

    /* Type-0x45 input buffer containing 1280x720 RGBA8 (minimum 0x384000). */
    const void *rgba_frame;
    size_t rgba_frame_size;

    /* Output storage for three returned u64 values; their meanings are undocumented. */
    uint64_t *output_values;
} CapsScProtoMovieMetaRequest;

/* Returns 0x68 for HOS 5-18, 0x78 for HOS 19+, and 0 when unsupported. */
size_t caps_sc_proto_rgba_input_size_for_firmware(unsigned major,
                                                  unsigned minor);

/* Pure validation, shared by the Switch adapter and host-side tests. */
bool caps_sc_proto_validate_rgba_request(const CapsScProtoMovieMetaRequest *request,
                                         size_t expected_input_data_size,
                                         char *error,
                                         size_t error_size);

#ifdef __SWITCH__
#include <switch.h>

/*
 * Experimental low-level wrapper for caps:sc cmd 1107.
 *
 * This invokes only the platform metadata generator. It does not construct
 * undocumented parameter blocks, interpret output_values, create MakerNotes,
 * or write to Album. The caller must supply correctly prepared input data.
 */
Result caps_sc_create_proto_movie_metadata_rgba_ex2(
    const CapsScProtoMovieMetaRequest *request);
#endif
