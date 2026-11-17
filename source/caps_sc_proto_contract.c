#include "caps_sc_proto.h"

#include <stdio.h>
#include <string.h>

static void set_error(char *error, size_t error_size, const char *message)
{
    if (error == NULL || error_size == 0)
        return;

    (void)snprintf(error, error_size, "%s",
                   message != NULL ? message : "Invalid caps:sc metadata request.");
}

size_t caps_sc_proto_rgba_input_size_for_firmware(unsigned major,
                                                  unsigned minor)
{
    (void)minor;

    if (major < 5)
        return 0;

    if (major < 19)
        return CAPS_SC_PROTO_INPUT_SIZE_HOS_5_TO_18;

    return CAPS_SC_PROTO_INPUT_SIZE_HOS_19_PLUS;
}

bool caps_sc_proto_validate_rgba_request(const CapsScProtoMovieMetaRequest *request,
                                         size_t expected_input_data_size,
                                         char *error,
                                         size_t error_size)
{
    CapsScProtoMovieMetaInputV5 input_prefix;

    if (request == NULL) {
        set_error(error, error_size, "Request pointer is null.");
        return false;
    }

    if (expected_input_data_size != CAPS_SC_PROTO_INPUT_SIZE_HOS_5_TO_18 &&
        expected_input_data_size != CAPS_SC_PROTO_INPUT_SIZE_HOS_19_PLUS) {
        set_error(error, error_size, "Unsupported caps:sc IPC input size.");
        return false;
    }

    if (request->output_values == NULL) {
        set_error(error, error_size, "Output-values pointer is null.");
        return false;
    }

    if (request->input_data == NULL ||
        request->input_data_size != expected_input_data_size) {
        set_error(error, error_size, "Scalar input block has the wrong size.");
        return false;
    }

    memcpy(&input_prefix, request->input_data, sizeof(input_prefix));
    if (input_prefix.width != 1280 || input_prefix.height != 720) {
        set_error(error, error_size, "caps:sc movie metadata requires 1280x720 dimensions.");
        return false;
    }

    if (request->input_buffer0 == NULL ||
        request->input_buffer0_size != CAPS_SC_PROTO_INPUT_BUFFER0_SIZE ||
        request->input_buffer1 == NULL ||
        request->input_buffer1_size != CAPS_SC_PROTO_INPUT_BUFFER1_SIZE) {
        set_error(error, error_size, "The fixed-size 0x400/0x404 input buffers are required.");
        return false;
    }

    if (expected_input_data_size == CAPS_SC_PROTO_INPUT_SIZE_HOS_19_PLUS) {
        if (request->input_buffer2 == NULL ||
            request->input_buffer2_size != CAPS_SC_PROTO_INPUT_BUFFER2_SIZE) {
            set_error(error, error_size, "HOS 19+ requires the additional 0x88 input buffer.");
            return false;
        }
    } else if (request->input_buffer2 != NULL || request->input_buffer2_size != 0) {
        set_error(error, error_size, "The 0x88 input buffer is only part of the HOS 19+ signature.");
        return false;
    }

    if (request->metadata_output == NULL ||
        request->metadata_output_size < CAPS_SC_PROTO_OUTPUT_BUFFER_MIN_SIZE) {
        set_error(error, error_size, "Metadata output buffer must be at least 0x7D000 bytes.");
        return false;
    }

    if (request->rgba_frame == NULL ||
        request->rgba_frame_size < CAPS_SC_PROTO_RGBA_FRAME_MIN_SIZE) {
        set_error(error, error_size, "RGBA frame buffer must be at least 1280x720 RGBA8.");
        return false;
    }

    set_error(error, error_size, "");
    return true;
}
