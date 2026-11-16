#define _POSIX_C_SOURCE 200809L

#include "video_probe.h"
#include "mp4_fixture.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


int main(void)
{
    uint8_t fixture[4096] = {0};
    char path[64];
    char error[160] = {0};
    Mp4Info info;
    size_t size = build_mp4(fixture, fourcc('a', 'v', 'c', '1'), false);

    write_temp_file(fixture, size, path);
    assert(mp4_probe_file(path, &info, error, sizeof(error)));
    assert(info.has_ftyp && info.has_moov && info.has_mdat);
    assert(info.has_video_track && !info.fragmented);
    assert(info.video_codec == Mp4VideoCodec_H264);
    assert(info.width == 1280 && info.height == 720);
    assert(info.duration_ms == 12500);
    assert(mp4_is_switch_720p_candidate(&info));
    unlink(path);

    size = build_mp4(fixture, fourcc('h', 'v', 'c', '1'), false);
    write_temp_file(fixture, size, path);
    assert(mp4_probe_file(path, &info, error, sizeof(error)));
    assert(info.video_codec == Mp4VideoCodec_HEVC);
    assert(!mp4_is_switch_720p_candidate(&info));
    unlink(path);

    size = build_mp4(fixture, fourcc('a', 'v', 'c', '1'), true);
    write_temp_file(fixture, size, path);
    assert(mp4_probe_file(path, &info, error, sizeof(error)));
    assert(info.fragmented);
    assert(!mp4_is_switch_720p_candidate(&info));
    unlink(path);

    {
        const uint8_t malformed[8] = {0, 0, 0, 0x40, 'f', 't', 'y', 'p'};
        write_temp_file(malformed, sizeof(malformed), path);
        assert(!mp4_probe_file(path, &info, error, sizeof(error)));
        unlink(path);
    }

    puts("All MP4 preflight tests passed.");
    return 0;
}
