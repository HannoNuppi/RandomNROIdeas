#include "caps_sc_proto.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint64_t test_output_values[3];

static void make_valid_request(CapsScProtoMovieMetaRequest *request,
                               const void *input_data,
                               size_t input_data_size,
                               const void *buffer0,
                               const void *buffer1,
                               const void *buffer2)
{
    static unsigned char dummy;
    memset(request, 0, sizeof(*request));
    request->input_data = input_data;
    request->input_data_size = input_data_size;
    request->input_buffer0 = buffer0;
    request->input_buffer0_size = CAPS_SC_PROTO_INPUT_BUFFER0_SIZE;
    request->input_buffer1 = buffer1;
    request->input_buffer1_size = CAPS_SC_PROTO_INPUT_BUFFER1_SIZE;
    request->input_buffer2 = buffer2;
    request->input_buffer2_size = buffer2 != NULL
        ? CAPS_SC_PROTO_INPUT_BUFFER2_SIZE : 0;
    request->metadata_output = &dummy;
    request->metadata_output_size = CAPS_SC_PROTO_OUTPUT_BUFFER_MIN_SIZE;
    request->rgba_frame = &dummy;
    request->rgba_frame_size = CAPS_SC_PROTO_RGBA_FRAME_MIN_SIZE;
    request->output_values = test_output_values;
}

int main(void)
{
    static unsigned char dummy;
    CapsScProtoMovieMetaInputV19 input_v19 = {0};
    CapsScProtoMovieMetaRequest request;
    char error[160] = {0};

    input_v19.base.width = 1280;
    input_v19.base.height = 720;

    assert(caps_sc_proto_rgba_input_size_for_firmware(4, 1) == 0);
    assert(caps_sc_proto_rgba_input_size_for_firmware(5, 0) == 0x68);
    assert(caps_sc_proto_rgba_input_size_for_firmware(18, 1) == 0x68);
    assert(caps_sc_proto_rgba_input_size_for_firmware(19, 0) == 0x78);
    assert(caps_sc_proto_rgba_input_size_for_firmware(23, 0) == 0x78);

    /* HOS 5-18: two input buffers, then metadata output and RGBA frame. */
    make_valid_request(&request, &input_v19.base, 0x68,
                       &dummy, &dummy, NULL);
    assert(caps_sc_proto_validate_rgba_request(&request, 0x68,
                                               error, sizeof(error)));
    assert(error[0] == '\0');

    /* The HOS 19+ signature adds both 0x10 scalar bytes and a third 0x88 buffer. */
    make_valid_request(&request, &input_v19, 0x78,
                       &dummy, &dummy, &dummy);
    assert(caps_sc_proto_validate_rgba_request(&request, 0x78,
                                               error, sizeof(error)));
    assert(error[0] == '\0');

    assert(!caps_sc_proto_validate_rgba_request(&request, 0x68,
                                                error, sizeof(error)));
    assert(strstr(error, "Scalar input block") != NULL);

    make_valid_request(&request, &input_v19.base, 0x68,
                       &dummy, &dummy, NULL);
    request.input_buffer1_size--;
    assert(!caps_sc_proto_validate_rgba_request(&request, 0x68,
                                                error, sizeof(error)));
    assert(strstr(error, "0x400/0x404") != NULL);

    make_valid_request(&request, &input_v19, 0x78,
                       &dummy, &dummy, NULL);
    assert(!caps_sc_proto_validate_rgba_request(&request, 0x78,
                                                error, sizeof(error)));
    assert(strstr(error, "HOS 19+") != NULL);

    make_valid_request(&request, &input_v19.base, 0x68,
                       &dummy, &dummy, &dummy);
    assert(!caps_sc_proto_validate_rgba_request(&request, 0x68,
                                                error, sizeof(error)));
    assert(strstr(error, "only part of the HOS 19+") != NULL);

    make_valid_request(&request, &input_v19.base, 0x68,
                       &dummy, &dummy, NULL);
    request.metadata_output_size = CAPS_SC_PROTO_OUTPUT_BUFFER_MIN_SIZE - 1;
    assert(!caps_sc_proto_validate_rgba_request(&request, 0x68,
                                                error, sizeof(error)));
    assert(strstr(error, "Metadata output buffer") != NULL);

    make_valid_request(&request, &input_v19, 0x78,
                       &dummy, &dummy, &dummy);
    request.rgba_frame_size = CAPS_SC_PROTO_RGBA_FRAME_MIN_SIZE - 1;
    assert(!caps_sc_proto_validate_rgba_request(&request, 0x78,
                                                error, sizeof(error)));
    assert(strstr(error, "RGBA frame buffer") != NULL);

    make_valid_request(&request, &input_v19.base, 0x68,
                       &dummy, &dummy, NULL);
    input_v19.base.width = 1920;
    assert(!caps_sc_proto_validate_rgba_request(&request, 0x68,
                                                error, sizeof(error)));
    assert(strstr(error, "1280x720") != NULL);
    input_v19.base.width = 1280;

    make_valid_request(&request, &input_v19.base, 0x68,
                       &dummy, &dummy, NULL);
    request.output_values = NULL;
    assert(!caps_sc_proto_validate_rgba_request(&request, 0x68,
                                                error, sizeof(error)));
    assert(strstr(error, "Output-values pointer") != NULL);

    puts("All caps:sc proto metadata contract tests passed.");
    return 0;
}
