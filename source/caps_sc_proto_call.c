#include <switch.h>

#include "caps_sc_proto.h"

#include <string.h>

_Static_assert(sizeof(CapsScProtoMovieMetaInputV5) == 0x68,
               "HOS 5-18 caps:sc scalar input must be 0x68 bytes");
_Static_assert(sizeof(CapsScProtoMovieMetaInputV19) == 0x78,
               "HOS 19+ caps:sc scalar input must be 0x78 bytes");

static Result caps_sc_dispatch_rgba(const CapsScProtoMovieMetaRequest *request,
                                    bool hos19_or_newer)
{
    struct {
        uint64_t values[3];
    } output = {0};

    Result result;

    if (hos19_or_newer) {
        struct {
            uint8_t bytes[CAPS_SC_PROTO_INPUT_SIZE_HOS_19_PLUS];
        } input = {0};

        memcpy(input.bytes, request->input_data, sizeof(input.bytes));

        result = serviceDispatchInOut(
            capsscGetServiceSession(),
            CAPS_SC_PROTO_RGBA_CMD,
            input,
            output,
            .buffer_attrs = {
                SfBufferAttr_In | SfBufferAttr_HipcMapAlias | SfBufferAttr_FixedSize,
                SfBufferAttr_In | SfBufferAttr_HipcMapAlias | SfBufferAttr_FixedSize,
                SfBufferAttr_In | SfBufferAttr_HipcMapAlias | SfBufferAttr_FixedSize,
                SfBufferAttr_Out | SfBufferAttr_HipcMapAlias,
                SfBufferAttr_In | SfBufferAttr_HipcMapAlias |
                    SfBufferAttr_HipcMapTransferAllowsNonSecure
            },
            .buffers = {
                { request->input_buffer0, request->input_buffer0_size },
                { request->input_buffer1, request->input_buffer1_size },
                { request->input_buffer2, request->input_buffer2_size },
                { request->metadata_output, request->metadata_output_size },
                { request->rgba_frame, request->rgba_frame_size }
            }
        );
    } else {
        struct {
            uint8_t bytes[CAPS_SC_PROTO_INPUT_SIZE_HOS_5_TO_18];
        } input = {0};

        memcpy(input.bytes, request->input_data, sizeof(input.bytes));

        result = serviceDispatchInOut(
            capsscGetServiceSession(),
            CAPS_SC_PROTO_RGBA_CMD,
            input,
            output,
            .buffer_attrs = {
                SfBufferAttr_In | SfBufferAttr_HipcMapAlias | SfBufferAttr_FixedSize,
                SfBufferAttr_In | SfBufferAttr_HipcMapAlias | SfBufferAttr_FixedSize,
                SfBufferAttr_Out | SfBufferAttr_HipcMapAlias,
                SfBufferAttr_In | SfBufferAttr_HipcMapAlias |
                    SfBufferAttr_HipcMapTransferAllowsNonSecure
            },
            .buffers = {
                { request->input_buffer0, request->input_buffer0_size },
                { request->input_buffer1, request->input_buffer1_size },
                { request->metadata_output, request->metadata_output_size },
                { request->rgba_frame, request->rgba_frame_size }
            }
        );
    }

    if (R_SUCCEEDED(result))
        memcpy(request->output_values, output.values, sizeof(output.values));

    return result;
}

Result caps_sc_create_proto_movie_metadata_rgba_ex2(
    const CapsScProtoMovieMetaRequest *request)
{
    const bool hos19_or_newer = hosversionAtLeast(19, 0, 0);
    const size_t expected_size = hos19_or_newer
        ? CAPS_SC_PROTO_INPUT_SIZE_HOS_19_PLUS
        : CAPS_SC_PROTO_INPUT_SIZE_HOS_5_TO_18;
    char validation_error[160];

    if (hosversionBefore(5, 0, 0))
        return MAKERESULT(Module_Libnx, LibnxError_IncompatSysVer);

    if (!caps_sc_proto_validate_rgba_request(
            request, expected_size, validation_error, sizeof(validation_error)))
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);

    {
        Result result = capsscInitialize();
        if (R_FAILED(result))
            return result;

        result = caps_sc_dispatch_rgba(request, hos19_or_newer);
        capsscExit();
        return result;
    }
}
