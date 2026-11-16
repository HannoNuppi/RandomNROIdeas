#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    Mp4VideoCodec_Unknown = 0,
    Mp4VideoCodec_H264,
    Mp4VideoCodec_HEVC,
    Mp4VideoCodec_AV1,
    Mp4VideoCodec_MPEG4
} Mp4VideoCodec;

typedef enum {
    Mp4AudioCodec_None = 0,
    Mp4AudioCodec_Unknown,
    Mp4AudioCodec_AAC,
    Mp4AudioCodec_AC3,
    Mp4AudioCodec_EAC3,
    Mp4AudioCodec_Opus
} Mp4AudioCodec;

typedef struct {
    uint64_t file_size;
    uint64_t duration_ms;
    unsigned width;
    unsigned height;
    Mp4VideoCodec video_codec;
    Mp4AudioCodec audio_codec;
    uint8_t avc_profile_idc;
    uint8_t avc_level_idc;
    char major_brand[5];
    bool has_ftyp;
    bool has_moov;
    bool has_mdat;
    bool has_video_track;
    bool has_audio_track;
    bool fragmented;
    bool moov_before_mdat;
} Mp4Info;

/*
 * Reads MP4 box headers and track metadata only. It never decodes or modifies
 * the input media. On success, out_info describes the first video/audio tracks.
 */
bool mp4_probe_file(const char *path,
                    Mp4Info *out_info,
                    char *error,
                    size_t error_size);

/*
 * Conservative first-pass target profile for the Switch Album pipeline:
 * non-fragmented MP4, H.264/AVC, exactly 1280x720, and either no audio or AAC.
 * Passing this check does NOT mean that the file has Album MakerNote/CMAC data.
 */
bool mp4_is_switch_720p_candidate(const Mp4Info *info);

const char *mp4_video_codec_name(Mp4VideoCodec codec);
const char *mp4_audio_codec_name(Mp4AudioCodec codec);
