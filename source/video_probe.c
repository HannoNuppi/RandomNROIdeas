#include "video_probe.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint64_t start;
    uint64_t payload;
    uint64_t end;
    uint32_t type;
} Mp4Box;

typedef struct {
    bool has_handler;
    bool is_video;
    bool is_audio;
    Mp4VideoCodec video_codec;
    Mp4AudioCodec audio_codec;
    unsigned width;
    unsigned height;
    uint8_t avc_profile_idc;
    uint8_t avc_level_idc;
} Mp4TrackInfo;

static uint32_t make_fourcc(char a, char b, char c, char d)
{
    return ((uint32_t)(uint8_t)a << 24) |
           ((uint32_t)(uint8_t)b << 16) |
           ((uint32_t)(uint8_t)c << 8) |
           (uint32_t)(uint8_t)d;
}

static uint16_t read_be16(const uint8_t bytes[2])
{
    return (uint16_t)(((uint16_t)bytes[0] << 8) | bytes[1]);
}

static uint32_t read_be32(const uint8_t bytes[4])
{
    return ((uint32_t)bytes[0] << 24) |
           ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8) |
           (uint32_t)bytes[3];
}

static uint64_t read_be64(const uint8_t bytes[8])
{
    return ((uint64_t)read_be32(bytes) << 32) |
           read_be32(bytes + 4);
}

static void set_error(char *error, size_t error_size, const char *message)
{
    if (error == NULL || error_size == 0)
        return;

    snprintf(error, error_size, "%s", message != NULL ? message : "Unknown MP4 error");
}

static bool read_at(FILE *file, uint64_t offset, void *buffer, size_t size)
{
    if (file == NULL || buffer == NULL || offset > (uint64_t)LONG_MAX)
        return false;

    if (fseek(file, (long)offset, SEEK_SET) != 0)
        return false;

    return fread(buffer, 1, size, file) == size;
}

/*
 * Reads one ISO BMFF box bounded by parent_end. Box size zero means "to the
 * end of this parent"; largesize and uuid headers are handled explicitly.
 */
static bool read_next_box(FILE *file,
                          uint64_t parent_end,
                          uint64_t *cursor,
                          Mp4Box *out_box)
{
    uint8_t header[16];
    uint64_t box_size;
    uint64_t header_size = 8;
    const uint64_t start = *cursor;

    if (start > parent_end || parent_end - start < 8)
        return false;

    if (!read_at(file, start, header, 8))
        return false;

    const uint32_t size32 = read_be32(header);
    const uint32_t type = read_be32(header + 4);

    if (size32 == 1) {
        if (parent_end - start < 16 || !read_at(file, start + 8, header + 8, 8))
            return false;
        box_size = read_be64(header + 8);
        header_size = 16;
    } else if (size32 == 0) {
        box_size = parent_end - start;
    } else {
        box_size = size32;
    }

    if (type == make_fourcc('u', 'u', 'i', 'd'))
        header_size += 16;

    if (box_size < header_size || box_size > parent_end - start)
        return false;

    out_box->start = start;
    out_box->payload = start + header_size;
    out_box->end = start + box_size;
    out_box->type = type;
    *cursor = out_box->end;
    return true;
}

static bool parse_movie_header(FILE *file, const Mp4Box *box, Mp4Info *info)
{
    uint8_t header[32];
    const uint64_t payload_size = box->end - box->payload;
    uint8_t version;
    uint32_t timescale;
    uint64_t duration;

    if (payload_size < 20 || !read_at(file, box->payload, header,
                                      payload_size < sizeof(header) ? (size_t)payload_size : sizeof(header)))
        return false;

    version = header[0];
    if (version == 0) {
        timescale = read_be32(header + 12);
        duration = read_be32(header + 16);
    } else if (version == 1) {
        if (payload_size < 32)
            return false;
        timescale = read_be32(header + 20);
        duration = read_be64(header + 24);
    } else {
        return false;
    }

    if (timescale == 0 || duration == UINT32_MAX || duration == UINT64_MAX) {
        info->duration_ms = 0;
        return true;
    }

    info->duration_ms = (duration / timescale) * 1000ULL +
                        ((duration % timescale) * 1000ULL) / timescale;
    return true;
}

static bool parse_handler(FILE *file, const Mp4Box *box, Mp4TrackInfo *track)
{
    uint8_t handler[4];

    if (box->end - box->payload < 12 ||
        !read_at(file, box->payload + 8, handler, sizeof(handler)))
        return false;

    const uint32_t type = read_be32(handler);
    track->has_handler = true;
    track->is_video = type == make_fourcc('v', 'i', 'd', 'e');
    track->is_audio = type == make_fourcc('s', 'o', 'u', 'n');
    return true;
}

static void parse_avc_configuration(FILE *file,
                                    const Mp4Box *box,
                                    Mp4TrackInfo *track)
{
    uint8_t avc[4];

    if (box->end - box->payload < sizeof(avc) ||
        !read_at(file, box->payload, avc, sizeof(avc)))
        return;

    if (avc[0] != 1)
        return;

    track->avc_profile_idc = avc[1];
    track->avc_level_idc = avc[3];
}

static bool parse_video_sample_entry(FILE *file,
                                     const Mp4Box *entry,
                                     Mp4TrackInfo *track)
{
    uint8_t dimensions[4];
    uint64_t cursor;
    const uint64_t payload_size = entry->end - entry->payload;

    if (payload_size < 78 ||
        !read_at(file, entry->payload + 24, dimensions, sizeof(dimensions)))
        return false;

    track->width = read_be16(dimensions);
    track->height = read_be16(dimensions + 2);

    if (entry->type == make_fourcc('a', 'v', 'c', '1') ||
        entry->type == make_fourcc('a', 'v', 'c', '3')) {
        track->video_codec = Mp4VideoCodec_H264;
    } else if (entry->type == make_fourcc('h', 'v', 'c', '1') ||
               entry->type == make_fourcc('h', 'e', 'v', '1')) {
        track->video_codec = Mp4VideoCodec_HEVC;
    } else if (entry->type == make_fourcc('a', 'v', '0', '1')) {
        track->video_codec = Mp4VideoCodec_AV1;
    } else if (entry->type == make_fourcc('m', 'p', '4', 'v')) {
        track->video_codec = Mp4VideoCodec_MPEG4;
    }

    cursor = entry->payload + 78;
    while (cursor < entry->end) {
        Mp4Box child;
        if (!read_next_box(file, entry->end, &cursor, &child))
            return false;

        if (child.type == make_fourcc('a', 'v', 'c', 'C'))
            parse_avc_configuration(file, &child, track);
    }

    return true;
}

static bool parse_audio_sample_entry(const Mp4Box *entry, Mp4TrackInfo *track)
{
    if (entry->type == make_fourcc('m', 'p', '4', 'a')) {
        track->audio_codec = Mp4AudioCodec_AAC;
    } else if (entry->type == make_fourcc('a', 'c', '-', '3')) {
        track->audio_codec = Mp4AudioCodec_AC3;
    } else if (entry->type == make_fourcc('e', 'c', '-', '3')) {
        track->audio_codec = Mp4AudioCodec_EAC3;
    } else if (entry->type == make_fourcc('O', 'p', 'u', 's')) {
        track->audio_codec = Mp4AudioCodec_Opus;
    } else {
        track->audio_codec = Mp4AudioCodec_Unknown;
    }

    return true;
}

static bool parse_sample_description(FILE *file,
                                     const Mp4Box *box,
                                     Mp4TrackInfo *track)
{
    uint8_t header[8];
    uint64_t cursor;
    uint32_t entry_count;
    Mp4Box entry;

    if (box->end - box->payload < sizeof(header) ||
        !read_at(file, box->payload, header, sizeof(header)))
        return false;

    entry_count = read_be32(header + 4);
    if (entry_count == 0)
        return true;

    cursor = box->payload + sizeof(header);
    if (!read_next_box(file, box->end, &cursor, &entry))
        return false;

    if (track->is_video)
        return parse_video_sample_entry(file, &entry, track);

    if (track->is_audio)
        return parse_audio_sample_entry(&entry, track);

    return true;
}

static bool parse_sample_table(FILE *file,
                               const Mp4Box *box,
                               Mp4TrackInfo *track)
{
    uint64_t cursor = box->payload;

    while (cursor < box->end) {
        Mp4Box child;
        if (!read_next_box(file, box->end, &cursor, &child))
            return false;

        if (child.type == make_fourcc('s', 't', 's', 'd') &&
            !parse_sample_description(file, &child, track))
            return false;
    }

    return true;
}

static bool parse_media_information(FILE *file,
                                    const Mp4Box *box,
                                    Mp4TrackInfo *track)
{
    uint64_t cursor = box->payload;

    while (cursor < box->end) {
        Mp4Box child;
        if (!read_next_box(file, box->end, &cursor, &child))
            return false;

        if (child.type == make_fourcc('s', 't', 'b', 'l') &&
            !parse_sample_table(file, &child, track))
            return false;
    }

    return true;
}

static bool parse_media_box(FILE *file,
                            const Mp4Box *box,
                            Mp4TrackInfo *track)
{
    uint64_t cursor = box->payload;
    Mp4Box minf_box;
    bool has_minf = false;

    memset(track, 0, sizeof(*track));
    track->video_codec = Mp4VideoCodec_Unknown;
    track->audio_codec = Mp4AudioCodec_Unknown;

    while (cursor < box->end) {
        Mp4Box child;
        if (!read_next_box(file, box->end, &cursor, &child))
            return false;

        if (child.type == make_fourcc('h', 'd', 'l', 'r')) {
            if (!parse_handler(file, &child, track))
                return false;
        } else if (child.type == make_fourcc('m', 'i', 'n', 'f')) {
            minf_box = child;
            has_minf = true;
        }
    }

    if (has_minf && (track->is_video || track->is_audio))
        return parse_media_information(file, &minf_box, track);

    return true;
}

static bool parse_track(FILE *file, const Mp4Box *box, Mp4TrackInfo *track)
{
    uint64_t cursor = box->payload;

    memset(track, 0, sizeof(*track));
    track->video_codec = Mp4VideoCodec_Unknown;
    track->audio_codec = Mp4AudioCodec_Unknown;

    while (cursor < box->end) {
        Mp4Box child;
        if (!read_next_box(file, box->end, &cursor, &child))
            return false;

        if (child.type == make_fourcc('m', 'd', 'i', 'a'))
            return parse_media_box(file, &child, track);
    }

    return true;
}

static bool parse_movie_box(FILE *file, const Mp4Box *box, Mp4Info *info)
{
    uint64_t cursor = box->payload;
    bool found_movie_header = false;

    while (cursor < box->end) {
        Mp4Box child;
        if (!read_next_box(file, box->end, &cursor, &child))
            return false;

        if (child.type == make_fourcc('m', 'v', 'h', 'd')) {
            if (!parse_movie_header(file, &child, info))
                return false;
            found_movie_header = true;
        } else if (child.type == make_fourcc('t', 'r', 'a', 'k')) {
            Mp4TrackInfo track;
            if (!parse_track(file, &child, &track))
                return false;

            if (track.is_video && !info->has_video_track) {
                info->has_video_track = true;
                info->video_codec = track.video_codec;
                info->width = track.width;
                info->height = track.height;
                info->avc_profile_idc = track.avc_profile_idc;
                info->avc_level_idc = track.avc_level_idc;
            } else if (track.is_audio && !info->has_audio_track) {
                info->has_audio_track = true;
                info->audio_codec = track.audio_codec;
            }
        } else if (child.type == make_fourcc('m', 'v', 'e', 'x')) {
            info->fragmented = true;
        }
    }

    (void)found_movie_header;
    return true;
}

bool mp4_probe_file(const char *path,
                    Mp4Info *out_info,
                    char *error,
                    size_t error_size)
{
    FILE *file;
    long end_position;
    uint64_t cursor = 0;
    bool seen_mdat = false;

    if (path == NULL || out_info == NULL) {
        set_error(error, error_size, "Invalid MP4 inspection arguments.");
        return false;
    }

    memset(out_info, 0, sizeof(*out_info));
    out_info->video_codec = Mp4VideoCodec_Unknown;
    out_info->audio_codec = Mp4AudioCodec_None;

    file = fopen(path, "rb");
    if (file == NULL) {
        set_error(error, error_size, "Could not open this video file.");
        return false;
    }

    if (fseek(file, 0, SEEK_END) != 0 ||
        (end_position = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        set_error(error, error_size, "Could not determine the video file size.");
        return false;
    }

    out_info->file_size = (uint64_t)end_position;
    if (out_info->file_size < 16) {
        fclose(file);
        set_error(error, error_size, "The file is too small to be a valid MP4.");
        return false;
    }

    while (cursor < out_info->file_size) {
        Mp4Box box;
        if (!read_next_box(file, out_info->file_size, &cursor, &box)) {
            fclose(file);
            set_error(error, error_size, "Invalid or truncated MP4 box structure.");
            return false;
        }

        if (box.type == make_fourcc('f', 't', 'y', 'p')) {
            uint8_t brand[4];
            if (box.end - box.payload < sizeof(brand) ||
                !read_at(file, box.payload, brand, sizeof(brand))) {
                fclose(file);
                set_error(error, error_size, "The MP4 ftyp box is incomplete.");
                return false;
            }
            memcpy(out_info->major_brand, brand, sizeof(brand));
            out_info->major_brand[4] = '\0';
            out_info->has_ftyp = true;
        } else if (box.type == make_fourcc('m', 'o', 'o', 'v')) {
            if (!seen_mdat)
                out_info->moov_before_mdat = true;
            out_info->has_moov = true;
            if (!parse_movie_box(file, &box, out_info)) {
                fclose(file);
                set_error(error, error_size, "Invalid MP4 movie or track metadata.");
                return false;
            }
        } else if (box.type == make_fourcc('m', 'd', 'a', 't')) {
            out_info->has_mdat = true;
            seen_mdat = true;
        } else if (box.type == make_fourcc('m', 'o', 'o', 'f')) {
            out_info->fragmented = true;
        }
    }

    fclose(file);

    if (!out_info->has_ftyp || !out_info->has_moov || !out_info->has_mdat) {
        set_error(error, error_size, "MP4 must contain ftyp, moov, and mdat boxes.");
        return false;
    }

    if (!out_info->has_video_track) {
        set_error(error, error_size, "No video track was found in this MP4.");
        return false;
    }

    if (!out_info->has_audio_track)
        out_info->audio_codec = Mp4AudioCodec_None;

    set_error(error, error_size, "");
    return true;
}

bool mp4_is_switch_720p_candidate(const Mp4Info *info)
{
    if (info == NULL || !info->has_ftyp || !info->has_moov ||
        !info->has_mdat || !info->has_video_track || info->fragmented)
        return false;

    if (info->video_codec != Mp4VideoCodec_H264 ||
        info->width != 1280 || info->height != 720 ||
        info->duration_ms == 0)
        return false;

    return info->audio_codec == Mp4AudioCodec_None ||
           info->audio_codec == Mp4AudioCodec_AAC;
}

const char *mp4_video_codec_name(Mp4VideoCodec codec)
{
    switch (codec) {
        case Mp4VideoCodec_H264: return "H.264 / AVC";
        case Mp4VideoCodec_HEVC: return "H.265 / HEVC";
        case Mp4VideoCodec_AV1: return "AV1";
        case Mp4VideoCodec_MPEG4: return "MPEG-4 Visual";
        default: return "Unknown";
    }
}

const char *mp4_audio_codec_name(Mp4AudioCodec codec)
{
    switch (codec) {
        case Mp4AudioCodec_None: return "No audio";
        case Mp4AudioCodec_AAC: return "AAC";
        case Mp4AudioCodec_AC3: return "AC-3";
        case Mp4AudioCodec_EAC3: return "E-AC-3";
        case Mp4AudioCodec_Opus: return "Opus";
        default: return "Unknown";
    }
}
