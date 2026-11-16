#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void write_be16(uint8_t *out, uint16_t value)
{
    out[0] = (uint8_t)(value >> 8);
    out[1] = (uint8_t)value;
}

static void write_be32(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)(value >> 24);
    out[1] = (uint8_t)(value >> 16);
    out[2] = (uint8_t)(value >> 8);
    out[3] = (uint8_t)value;
}

static uint32_t fourcc(char a, char b, char c, char d)
{
    return ((uint32_t)(uint8_t)a << 24) |
           ((uint32_t)(uint8_t)b << 16) |
           ((uint32_t)(uint8_t)c << 8) |
           (uint32_t)(uint8_t)d;
}

static size_t append_box(uint8_t *out,
                         uint32_t type,
                         const uint8_t *payload,
                         size_t payload_size)
{
    write_be32(out, (uint32_t)(payload_size + 8));
    write_be32(out + 4, type);
    if (payload_size != 0)
        memcpy(out + 8, payload, payload_size);
    return payload_size + 8;
}

static size_t build_mp4(uint8_t *out, uint32_t video_type, bool fragmented)
{
    uint8_t ftyp_payload[8] = {'i', 's', 'o', 'm', 0, 0, 0, 0};
    uint8_t mvhd_payload[20] = {0};
    uint8_t hdlr_payload[12] = {0};
    uint8_t avcc_payload[4] = {1, 0x64, 0, 0x1f};
    uint8_t visual_payload[96] = {0};
    uint8_t sample_entry[128] = {0};
    uint8_t stsd_payload[256] = {0};
    uint8_t stsd[300] = {0};
    uint8_t stbl[340] = {0};
    uint8_t minf[380] = {0};
    uint8_t hdlr[32] = {0};
    uint8_t mdia_payload[512] = {0};
    uint8_t mdia[540] = {0};
    uint8_t trak[580] = {0};
    uint8_t mvhd[40] = {0};
    uint8_t moov_payload[1024] = {0};
    uint8_t moov[1050] = {0};
    uint8_t mdat_payload[16] = {0};
    uint8_t ftyp[32] = {0};
    uint8_t mdat[32] = {0};
    uint8_t mvex[8] = {0};
    size_t visual_size;
    size_t sample_entry_size;
    size_t stsd_size;
    size_t stbl_size;
    size_t minf_size;
    size_t hdlr_size;
    size_t mdia_payload_size = 0;
    size_t mdia_size;
    size_t trak_size;
    size_t mvhd_size;
    size_t moov_payload_size = 0;
    size_t moov_size;
    size_t ftyp_size;
    size_t mdat_size;
    size_t total = 0;

    write_be32(mvex, 8);
    write_be32(mvex + 4, fourcc('m', 'v', 'e', 'x'));

    write_be32(mvhd_payload + 12, 1000);
    write_be32(mvhd_payload + 16, 12500);
    mvhd_size = append_box(mvhd, fourcc('m', 'v', 'h', 'd'),
                           mvhd_payload, sizeof(mvhd_payload));

    write_be16(visual_payload + 24, 1280);
    write_be16(visual_payload + 26, 720);
    visual_size = append_box(visual_payload + 78, fourcc('a', 'v', 'c', 'C'),
                             avcc_payload, sizeof(avcc_payload));
    sample_entry_size = append_box(sample_entry, video_type,
                                   visual_payload, 78 + visual_size);

    write_be32(stsd_payload + 4, 1);
    memcpy(stsd_payload + 8, sample_entry, sample_entry_size);
    stsd_size = append_box(stsd, fourcc('s', 't', 's', 'd'),
                           stsd_payload, 8 + sample_entry_size);
    stbl_size = append_box(stbl, fourcc('s', 't', 'b', 'l'), stsd, stsd_size);
    minf_size = append_box(minf, fourcc('m', 'i', 'n', 'f'), stbl, stbl_size);

    memcpy(hdlr_payload + 8, "vide", 4);
    hdlr_size = append_box(hdlr, fourcc('h', 'd', 'l', 'r'),
                           hdlr_payload, sizeof(hdlr_payload));

    memcpy(mdia_payload + mdia_payload_size, hdlr, hdlr_size);
    mdia_payload_size += hdlr_size;
    memcpy(mdia_payload + mdia_payload_size, minf, minf_size);
    mdia_payload_size += minf_size;
    mdia_size = append_box(mdia, fourcc('m', 'd', 'i', 'a'),
                           mdia_payload, mdia_payload_size);
    trak_size = append_box(trak, fourcc('t', 'r', 'a', 'k'), mdia, mdia_size);

    memcpy(moov_payload + moov_payload_size, mvhd, mvhd_size);
    moov_payload_size += mvhd_size;
    memcpy(moov_payload + moov_payload_size, trak, trak_size);
    moov_payload_size += trak_size;
    if (fragmented) {
        memcpy(moov_payload + moov_payload_size, mvex, sizeof(mvex));
        moov_payload_size += sizeof(mvex);
    }
    moov_size = append_box(moov, fourcc('m', 'o', 'o', 'v'),
                           moov_payload, moov_payload_size);
    ftyp_size = append_box(ftyp, fourcc('f', 't', 'y', 'p'),
                           ftyp_payload, sizeof(ftyp_payload));
    mdat_size = append_box(mdat, fourcc('m', 'd', 'a', 't'),
                           mdat_payload, sizeof(mdat_payload));

    memcpy(out + total, ftyp, ftyp_size);
    total += ftyp_size;
    memcpy(out + total, moov, moov_size);
    total += moov_size;
    memcpy(out + total, mdat, mdat_size);
    total += mdat_size;
    return total;
}

static void write_temp_file(const uint8_t *data, size_t size, char path[64])
{
    int fd;
    FILE *file;

    strcpy(path, "/tmp/albumsigner-mp4-XXXXXX");
    fd = mkstemp(path);
    assert(fd >= 0);

    file = fdopen(fd, "wb");
    assert(file != NULL);
    assert(fwrite(data, 1, size, file) == size);
    assert(fclose(file) == 0);
}
