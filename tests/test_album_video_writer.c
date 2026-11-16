#define _POSIX_C_SOURCE 200809L

#include "album_video_writer.h"
#include "mp4_fixture.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint64_t expected_application_id;
    uint64_t data_size;
    uint64_t data_written;
    uint64_t meta_written;
    unsigned data_calls;
    unsigned meta_calls;
    unsigned finish_calls;
    unsigned commit_calls;
    unsigned discard_calls;
    bool opened;
    unsigned phase;
    bool fail_first_meta_write;
} FakeAlbumService;

static uint32_t fake_open(void *context, uint64_t app_id, uint64_t *out_stream)
{
    FakeAlbumService *fake = (FakeAlbumService *)context;
    assert(app_id == fake->expected_application_id);
    assert(!fake->opened);
    fake->opened = true;
    fake->phase = 1;
    *out_stream = 0x12345678;
    return 0;
}

static uint32_t fake_set_size(void *context, uint64_t stream, uint64_t size)
{
    FakeAlbumService *fake = (FakeAlbumService *)context;
    assert(fake->opened && stream == 0x12345678);
    assert(fake->phase == 1);
    fake->phase = 2;
    fake->data_size = size;
    return 0;
}

static uint32_t fake_start_data(void *context, uint64_t stream)
{
    FakeAlbumService *fake = (FakeAlbumService *)context;
    assert(fake->opened && stream == 0x12345678 && fake->phase == 2);
    fake->phase = 3;
    return 0;
}

static uint32_t fake_end_data(void *context, uint64_t stream)
{
    FakeAlbumService *fake = (FakeAlbumService *)context;
    assert(fake->opened && stream == 0x12345678 && fake->phase == 3);
    fake->phase = 4;
    return 0;
}

static uint32_t fake_start_meta(void *context, uint64_t stream)
{
    FakeAlbumService *fake = (FakeAlbumService *)context;
    assert(fake->opened && stream == 0x12345678 && fake->phase == 4);
    fake->phase = 5;
    return 0;
}

static uint32_t fake_end_meta(void *context, uint64_t stream)
{
    FakeAlbumService *fake = (FakeAlbumService *)context;
    assert(fake->opened && stream == 0x12345678 && fake->phase == 5);
    fake->phase = 6;
    return 0;
}

static uint32_t fake_write_data(void *context, uint64_t stream,
                                uint64_t offset, const void *buffer, uint64_t size)
{
    FakeAlbumService *fake = (FakeAlbumService *)context;
    assert(fake->opened && stream == 0x12345678 && fake->phase == 3);
    assert(buffer != NULL && size > 0);
    assert(offset == fake->data_written);
    fake->data_written += size;
    fake->data_calls++;
    return 0;
}

static uint32_t fake_write_meta(void *context, uint64_t stream,
                                uint64_t offset, const void *buffer, uint64_t size)
{
    FakeAlbumService *fake = (FakeAlbumService *)context;
    assert(fake->opened && stream == 0x12345678 && fake->phase == 5);
    assert(buffer != NULL && size > 0);
    assert(offset == fake->meta_written);
    fake->meta_calls++;
    if (fake->fail_first_meta_write && fake->meta_calls == 1)
        return 0xAABBCCDDu;
    fake->meta_written += size;
    return 0;
}

static uint32_t fake_finish(void *context, uint64_t stream)
{
    FakeAlbumService *fake = (FakeAlbumService *)context;
    assert(fake->opened && stream == 0x12345678 && fake->phase == 6);
    fake->phase = 7;
    fake->finish_calls++;
    return 0;
}

static uint32_t fake_commit(void *context, uint64_t stream)
{
    FakeAlbumService *fake = (FakeAlbumService *)context;
    assert(fake->opened && stream == 0x12345678 && fake->phase == 7);
    fake->phase = 8;
    fake->commit_calls++;
    fake->opened = false;
    return 0;
}

static uint32_t fake_discard(void *context, uint64_t stream)
{
    FakeAlbumService *fake = (FakeAlbumService *)context;
    assert(fake->opened && stream == 0x12345678 && fake->phase >= 1 && fake->phase < 8);
    fake->phase = 9;
    fake->discard_calls++;
    fake->opened = false;
    return 0;
}

static AlbumVideoWriteOps make_ops(FakeAlbumService *fake)
{
    AlbumVideoWriteOps ops = {
        .context = fake,
        .open = fake_open,
        .set_data_size = fake_set_size,
        .start_data_section = fake_start_data,
        .write_data = fake_write_data,
        .end_data_section = fake_end_data,
        .start_meta_section = fake_start_meta,
        .write_meta = fake_write_meta,
        .end_meta_section = fake_end_meta,
        .finish = fake_finish,
        .commit = fake_commit,
        .discard = fake_discard
    };
    return ops;
}

static size_t append_free_box(uint8_t *buffer, size_t capacity, size_t offset,
                              size_t payload_size)
{
    assert(offset + payload_size + 8 <= capacity);
    write_be32(buffer + offset, (uint32_t)(payload_size + 8));
    write_be32(buffer + offset + 4, fourcc('f', 'r', 'e', 'e'));
    memset(buffer + offset + 8, 0x5A, payload_size);
    return offset + payload_size + 8;
}

int main(void)
{
    const uint64_t app_id = 0x0100AABBCCDDEEFFULL;
    const size_t fixture_capacity = 1024 * 1024;
    const size_t metadata_size = 0x50000;
    uint8_t *fixture = (uint8_t *)calloc(1, fixture_capacity);
    uint8_t *metadata_bytes = (uint8_t *)malloc(metadata_size);
    char mp4_path[64];
    char metadata_path[64];
    char error[256] = {0};
    FakeAlbumService fake = {0};
    AlbumVideoWriteOps ops;
    size_t mp4_size;

    assert(fixture != NULL && metadata_bytes != NULL);
    memset(metadata_bytes, 0xA5, metadata_size);

    mp4_size = build_mp4(fixture, fourcc('a', 'v', 'c', '1'), false);
    mp4_size = append_free_box(fixture, fixture_capacity, mp4_size, 0x50000);
    write_temp_file(fixture, mp4_size, mp4_path);
    write_temp_file(metadata_bytes, metadata_size, metadata_path);

    fake.expected_application_id = app_id;
    ops = make_ops(&fake);

    assert(album_video_write_prepared(
        mp4_path, metadata_path, app_id, &ops, error, sizeof(error)
    ));
    assert(!fake.opened && fake.phase == 8);
    assert(fake.data_size == mp4_size);
    assert(fake.data_written == mp4_size);
    assert(fake.meta_written == metadata_size);
    assert(fake.data_calls == 2);
    assert(fake.meta_calls == 2);
    assert(fake.finish_calls == 1);
    assert(fake.commit_calls == 1);
    assert(fake.discard_calls == 0);

    memset(&fake, 0, sizeof(fake));
    fake.expected_application_id = app_id;
    fake.fail_first_meta_write = true;
    ops = make_ops(&fake);
    memset(error, 0, sizeof(error));

    assert(!album_video_write_prepared(
        mp4_path, metadata_path, app_id, &ops, error, sizeof(error)
    ));
    assert(!fake.opened && fake.phase == 9);
    assert(fake.discard_calls == 1);
    assert(fake.commit_calls == 0);
    assert(strstr(error, "Album metadata write failed") != NULL);

    unlink(mp4_path);
    unlink(metadata_path);
    free(fixture);
    free(metadata_bytes);
    puts("All Album video writer tests passed.");
    return 0;
}
