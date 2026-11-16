#include <switch.h>

#include "album_video_caps_backend.h"
#include "album_video_writer.h"

#include <string.h>

static Result switch_album_open(void *context,
                                uint64_t application_id,
                                uint64_t *out_stream)
{
    CapsApplicationId caps_application = {0};
    CapsAlbumFileId file_id = {0};
    Result result;

    (void)context;

    if (hosversionBefore(4, 0, 0))
        return MAKERESULT(Module_Libnx, LibnxError_IncompatSysVer);

    result = capscInitialize();
    if (R_FAILED(result))
        return result;

    caps_application.application_id = application_id;
    result = capscGenerateCurrentAlbumFileId(
        &caps_application, CapsAlbumFileContents_Movie, &file_id
    );

    if (R_SUCCEEDED(result))
        result = capscOpenAlbumMovieWriteStream(out_stream, &file_id);

    if (R_FAILED(result))
        capscExit();

    return result;
}

static Result switch_album_set_data_size(void *context,
                                         uint64_t stream,
                                         uint64_t size)
{
    (void)context;
    return capscSetAlbumMovieWriteStreamDataSize(stream, size);
}

static Result switch_album_start_data(void *context, uint64_t stream)
{
    (void)context;
    return capscStartAlbumMovieWriteStreamDataSection(stream);
}

static Result switch_album_write_data(void *context,
                                      uint64_t stream,
                                      uint64_t offset,
                                      const void *buffer,
                                      uint64_t size)
{
    (void)context;
    return capscWriteDataToAlbumMovieWriteStream(stream, offset, buffer, size);
}

static Result switch_album_end_data(void *context, uint64_t stream)
{
    (void)context;
    return capscEndAlbumMovieWriteStreamDataSection(stream);
}

static Result switch_album_start_meta(void *context, uint64_t stream)
{
    (void)context;
    return capscStartAlbumMovieWriteStreamMetaSection(stream);
}

static Result switch_album_write_meta(void *context,
                                      uint64_t stream,
                                      uint64_t offset,
                                      const void *buffer,
                                      uint64_t size)
{
    (void)context;
    return capscWriteMetaToAlbumMovieWriteStream(stream, offset, buffer, size);
}

static Result switch_album_end_meta(void *context, uint64_t stream)
{
    (void)context;
    return capscEndAlbumMovieWriteStreamMetaSection(stream);
}

static Result switch_album_finish(void *context, uint64_t stream)
{
    (void)context;
    return capscFinishAlbumMovieWriteStream(stream);
}

static Result switch_album_commit(void *context, uint64_t stream)
{
    Result result;

    (void)context;
    result = capscCommitAlbumMovieWriteStream(stream);

    if (R_SUCCEEDED(result))
        capscExit();

    return result;
}

static Result switch_album_discard(void *context, uint64_t stream)
{
    Result result;

    (void)context;
    result = capscDiscardAlbumMovieWriteStream(stream);
    capscExit();
    return result;
}

bool album_video_import_prepared_on_switch(const char *mp4_path,
                                           const char *metadata_path,
                                           uint64_t application_id,
                                           char *error,
                                           size_t error_size)
{
    static const AlbumVideoWriteOps ops = {
        .context = NULL,
        .open = switch_album_open,
        .set_data_size = switch_album_set_data_size,
        .start_data_section = switch_album_start_data,
        .write_data = switch_album_write_data,
        .end_data_section = switch_album_end_data,
        .start_meta_section = switch_album_start_meta,
        .write_meta = switch_album_write_meta,
        .end_meta_section = switch_album_end_meta,
        .finish = switch_album_finish,
        .commit = switch_album_commit,
        .discard = switch_album_discard
    };

    return album_video_write_prepared(
        mp4_path, metadata_path, application_id, &ops, error, error_size
    );
}
