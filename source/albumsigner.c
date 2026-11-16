#include <switch.h>

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "albumsigner.h"
#include "image_decode.h"
#include "video_probe.h"

#define MAX_TITLES       128
#define MAX_FILES        128
#define VISIBLE_ROWS     16
#define PATH_BUFFER_SIZE 512
#define NAME_BUFFER_SIZE 256

typedef struct {
    u64 application_id;
    char name[NAME_BUFFER_SIZE];
} InstalledTitle;

typedef struct {
    char name[NAME_BUFFER_SIZE];
    bool is_directory;
} BrowserEntry;

static bool has_supported_extension(const char *name)
{
    const char *dot = strrchr(name, '.');
    if (dot == NULL)
        return false;

    return strcasecmp(dot, ".jpg") == 0 ||
           strcasecmp(dot, ".jpeg") == 0 ||
           strcasecmp(dot, ".png") == 0 ||
           strcasecmp(dot, ".mp4") == 0;
}

static bool is_mp4_path(const char *path)
{
    const char *dot = path != NULL ? strrchr(path, '.') : NULL;
    return dot != NULL && strcasecmp(dot, ".mp4") == 0;
}

static bool join_path(char *out, size_t out_size, const char *base, const char *name)
{
    const size_t base_len = strlen(base);
    const char *separator = (base_len > 0 && base[base_len - 1] == '/') ? "" : "/";
    const int written = snprintf(out, out_size, "%s%s%s", base, separator, name);
    return written >= 0 && (size_t)written < out_size;
}

static void parent_directory(char *path)
{
    size_t length = strlen(path);

    while (length > 0 && path[length - 1] == '/')
        path[--length] = '\0';

    char *last_slash = strrchr(path, '/');
    if (last_slash == NULL)
        return;

    if (last_slash == path + 5) {
        path[6] = '\0';
        return;
    }

    last_slash[1] = '\0';
}

static int load_directory(const char *path, BrowserEntry *entries, int capacity, bool *out_ok)
{
    DIR *directory = opendir(path);
    if (directory == NULL) {
        *out_ok = false;
        return 0;
    }

    int count = 0;
    if (strcmp(path, "sdmc:/") != 0 && count < capacity) {
        snprintf(entries[count].name, sizeof(entries[count].name), "..");
        entries[count].is_directory = true;
        count++;
    }

    struct dirent *item;
    while ((item = readdir(directory)) != NULL && count < capacity) {
        if (strcmp(item->d_name, ".") == 0 || strcmp(item->d_name, "..") == 0)
            continue;

        char full_path[PATH_BUFFER_SIZE];
        if (!join_path(full_path, sizeof(full_path), path, item->d_name))
            continue;

        struct stat info;
        if (stat(full_path, &info) != 0)
            continue;

        const bool is_directory = S_ISDIR(info.st_mode);
        if (!is_directory && !has_supported_extension(item->d_name))
            continue;

        snprintf(entries[count].name, sizeof(entries[count].name), "%s", item->d_name);
        entries[count].is_directory = is_directory;
        count++;
    }

    closedir(directory);
    *out_ok = true;
    return count;
}

static bool browse_for_media(char *selected_path, size_t selected_path_size)
{
    char current_path[PATH_BUFFER_SIZE] = "sdmc:/";
    BrowserEntry entries[MAX_FILES];
    int selection = 0;
    PadState pad;
    padInitializeDefault(&pad);

    while (appletMainLoop()) {
        bool directory_ok = false;
        const int count = load_directory(current_path, entries, MAX_FILES, &directory_ok);

        consoleClear();
        printf("AlbumSigner - Select media\n");
        printf("===========================\n");
        printf("SD: %s\n\n", current_path);

        if (!directory_ok) {
            printf("Could not open this folder.\n");
            printf("B: parent folder    +: exit\n");
        } else {
            if (count == 0)
                printf("(No supported files or folders here.)\n");

            int first = 0;
            if (selection >= VISIBLE_ROWS)
                first = selection - VISIBLE_ROWS + 1;

            for (int i = first; i < count && i < first + VISIBLE_ROWS; i++) {
                printf("%c %s %s\n",
                       i == selection ? '>' : ' ',
                       entries[i].is_directory ? "[DIR]" : "     ",
                       entries[i].name);
            }

            printf("\nA: open/select   B: parent   +: exit\n");
            printf("Inputs: JPG/JPEG/PNG images; MP4 video inspection.\n");
        }
        consoleUpdate(NULL);

        bool redraw = false;
        while (appletMainLoop() && !redraw) {
            padUpdate(&pad);
            const u64 down = padGetButtonsDown(&pad);

            if (down & HidNpadButton_Plus)
                return false;

            if (down & HidNpadButton_B) {
                if (strcmp(current_path, "sdmc:/") != 0) {
                    parent_directory(current_path);
                    selection = 0;
                }
                redraw = true;
            } else if (directory_ok && count > 0 && (down & HidNpadButton_Down)) {
                if (selection < count - 1)
                    selection++;
                redraw = true;
            } else if (directory_ok && count > 0 && (down & HidNpadButton_Up)) {
                if (selection > 0)
                    selection--;
                redraw = true;
            } else if (directory_ok && count > 0 && (down & HidNpadButton_A)) {
                char next_path[PATH_BUFFER_SIZE];
                if (!join_path(next_path, sizeof(next_path), current_path, entries[selection].name)) {
                    redraw = true;
                    continue;
                }

                if (entries[selection].is_directory) {
                    if (strcmp(entries[selection].name, "..") == 0)
                        parent_directory(current_path);
                    else
                        snprintf(current_path, sizeof(current_path), "%s", next_path);
                    selection = 0;
                } else {
                    snprintf(selected_path, selected_path_size, "%s", next_path);
                    return true;
                }
                redraw = true;
            }

            consoleUpdate(NULL);
        }
    }

    return false;
}

static int load_installed_titles(InstalledTitle *titles, int capacity, Result *out_result)
{
    NsApplicationRecord records[MAX_TITLES];
    s32 record_count = 0;

    *out_result = nsInitialize();
    if (R_FAILED(*out_result))
        return 0;

    *out_result = nsListApplicationRecord(records, capacity, 0, &record_count);
    if (R_FAILED(*out_result)) {
        nsExit();
        return 0;
    }

    if (record_count > capacity)
        record_count = capacity;

    int title_count = 0;
    NsApplicationControlData *control = malloc(sizeof(*control));
    if (control == NULL) {
        *out_result = MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
        nsExit();
        return 0;
    }

    for (s32 i = 0; i < record_count; i++) {
        u64 actual_size = 0;
        const u64 application_id = records[i].application_id;

        if (application_id == 0)
            continue;

        memset(control, 0, sizeof(*control));
        Result rc = nsGetApplicationControlData(
            NsApplicationControlSource_Storage,
            application_id,
            control,
            sizeof(*control),
            &actual_size
        );

        titles[title_count].application_id = application_id;
        if (R_SUCCEEDED(rc)) {
            NacpLanguageEntry *language_entry = NULL;
            rc = nsGetApplicationDesiredLanguage(&control->nacp, &language_entry);
            if (R_SUCCEEDED(rc) && language_entry != NULL && language_entry->name[0] != '\0') {
                snprintf(titles[title_count].name, sizeof(titles[title_count].name),
                         "%.*s", (int)sizeof(titles[title_count].name) - 1,
                         language_entry->name);
            } else {
                snprintf(titles[title_count].name, sizeof(titles[title_count].name),
                         "Unknown title");
            }
        } else {
            snprintf(titles[title_count].name, sizeof(titles[title_count].name),
                     "Title metadata unavailable");
        }

        title_count++;
        if (title_count >= capacity)
            break;
    }

    free(control);
    nsExit();
    *out_result = 0;
    return title_count;
}

static bool select_installed_title(const InstalledTitle *titles, int count, int *out_selection)
{
    if (count <= 0)
        return false;

    int selection = 0;
    PadState pad;
    padInitializeDefault(&pad);

    while (appletMainLoop()) {
        consoleClear();
        printf("AlbumSigner - Select game\n");
        printf("==========================\n");
        printf("Installed games on this console:\n\n");

        int first = 0;
        if (selection >= VISIBLE_ROWS)
            first = selection - VISIBLE_ROWS + 1;

        for (int i = first; i < count && i < first + VISIBLE_ROWS; i++) {
            printf("%c %s\n",
                   i == selection ? '>' : ' ',
                   titles[i].name);
            printf("    %016llX\n",
                   (unsigned long long)titles[i].application_id);
        }

        printf("\nA: choose game   B: back   +: exit\n");
        consoleUpdate(NULL);

        bool redraw = false;
        while (appletMainLoop() && !redraw) {
            padUpdate(&pad);
            const u64 down = padGetButtonsDown(&pad);

            if (down & HidNpadButton_Plus)
                return false;
            if (down & HidNpadButton_B)
                return false;
            if ((down & HidNpadButton_Down) && selection < count - 1) {
                selection++;
                redraw = true;
            } else if ((down & HidNpadButton_Up) && selection > 0) {
                selection--;
                redraw = true;
            } else if (down & HidNpadButton_A) {
                *out_selection = selection;
                return true;
            }
        }
    }

    return false;
}

static bool get_current_local_time(TimeCalendarTime *calendar, TimeCalendarAdditionalInfo *info)
{
    u64 timestamp = 0;
    if (R_FAILED(timeGetCurrentTime(TimeType_UserSystemClock, &timestamp)))
        return false;

    return R_SUCCEEDED(timeToCalendarTimeWithMyRule(timestamp, calendar, info));
}

static void show_preview(const char *media_path, const InstalledTitle *title)
{
    TimeCalendarTime calendar = {0};
    TimeCalendarAdditionalInfo timezone = {0};
    const bool time_ok = get_current_local_time(&calendar, &timezone);
    const bool is_video = is_mp4_path(media_path);

    consoleClear();
    printf("AlbumSigner - %s preflight\n", is_video ? "Video" : "Image");
    printf("================================\n\n");
    printf("%s:\n%s\n\n", is_video ? "Video file" : "Image file", media_path);
    printf("Selected game:\n%s\n", title->name);
    printf("Application Title ID: %016llX\n\n",
           (unsigned long long)title->application_id);

    if (time_ok) {
        printf("%s\n",
               is_video
                   ? "Current local console time (planned Album timestamp):"
                   : "The Switch will timestamp the new Album entry using its local clock:");
        printf("%04u-%02u-%02u %02u:%02u:%02u\n",
               calendar.year, calendar.month, calendar.day,
               calendar.hour, calendar.minute, calendar.second);
        printf("Timezone: %.*s (UTC offset %d seconds)\n\n",
               (int)sizeof(timezone.timezoneName),
               timezone.timezoneName, timezone.offset);
    } else {
        printf("Could not read the console's local time.\n\n");
    }

    if (is_video) {
        Mp4Info info;
        char probe_error[192] = {0};

        if (mp4_probe_file(media_path, &info, probe_error, sizeof(probe_error))) {
            printf("MP4 container: valid basic box structure\n");
            printf("Major brand: %s\n", info.major_brand[0] ? info.major_brand : "unknown");
            printf("File size: %llu bytes\n",
                   (unsigned long long)info.file_size);
            printf("Duration: %llu.%03llu seconds\n",
                   (unsigned long long)(info.duration_ms / 1000ULL),
                   (unsigned long long)(info.duration_ms % 1000ULL));
            printf("Video: %s, %ux%u\n",
                   mp4_video_codec_name(info.video_codec), info.width, info.height);
            if (info.video_codec == Mp4VideoCodec_H264 && info.avc_profile_idc != 0) {
                printf("AVC profile/level: %u / %u\n",
                       info.avc_profile_idc, info.avc_level_idc);
            }
            printf("Audio: %s\n", mp4_audio_codec_name(info.audio_codec));
            printf("Fragmented MP4: %s\n", info.fragmented ? "yes" : "no");
            printf("Fast-start layout: %s\n",
                   info.moov_before_mdat ? "yes" : "no");

            if (mp4_is_switch_720p_candidate(&info)) {
                printf("\nBase input profile: H.264 1280x720 - candidate\n");
            } else {
                printf("\nBase input profile: not yet suitable for the first video path\n");
            }
        } else {
            printf("MP4 inspection failed:\n%s\n", probe_error);
        }

        printf("\nVideo Album writing is not enabled yet.\n");
        printf("The MP4 is only inspected; no data is changed or added.\n");
        printf("\nA: Run MP4 preflight check   B: choose another file   +: exit\n");
    } else {
        printf("The Switch system Album service will convert and save\n");
        printf("the image as a genuine Switch screenshot entry.\n");
        printf("The original file will remain unchanged.\n\n");
        printf("A: Add image to Album   B: choose another image   +: exit\n");
    }

    consoleUpdate(NULL);
}

static bool save_image_to_album(const char *media_path,
                                const InstalledTitle *title,
                                char *message,
                                size_t message_size)
{
    uint8_t *source_pixels = NULL;
    uint8_t *album_pixels = NULL;
    unsigned source_width = 0;
    unsigned source_height = 0;
    char decode_error[192] = {0};

    if (!hosversionAtLeast(4, 0, 0)) {
        snprintf(message, message_size,
                 "Album image saving requires system version 4.0.0 or newer.");
        return false;
    }

    if (!image_decode_to_rgba(media_path, &source_pixels,
                              &source_width, &source_height,
                              decode_error, sizeof(decode_error))) {
        snprintf(message, message_size, "Image decode failed: %s", decode_error);
        return false;
    }

    if (!image_fit_to_album_rgba(source_pixels, source_width, source_height,
                                 &album_pixels, decode_error, sizeof(decode_error))) {
        free(source_pixels);
        snprintf(message, message_size, "Image conversion failed: %s", decode_error);
        return false;
    }

    free(source_pixels);
    source_pixels = NULL;

    AppletIdentityInfo identity;
    memset(&identity, 0, sizeof(identity));
    identity.appletId = AppletId_application;
    identity.application_id = title->application_id;

    Result rc = appletSetScreenShotPermission(AppletScreenShotPermission_Enable);
    if (R_FAILED(rc)) {
        free(album_pixels);
        snprintf(message, message_size,
                 "Could not enable screenshot saving (0x%08X).", rc);
        return false;
    }

    rc = appletSetScreenShotAppletIdentityInfo(&identity);
    if (R_FAILED(rc)) {
        free(album_pixels);
        snprintf(message, message_size,
                 "Could not set the selected game's Album identity (0x%08X).", rc);
        return false;
    }

    rc = capssuInitialize();
    if (R_FAILED(rc)) {
        free(album_pixels);
        snprintf(message, message_size,
                 "Could not initialize the Switch Album service (0x%08X).", rc);
        return false;
    }

    CapsApplicationAlbumEntry entry;
    memset(&entry, 0, sizeof(entry));

    rc = capssuSaveScreenShot(
        album_pixels,
        ALBUM_IMAGE_RGBA_SIZE,
        AlbumReportOption_Disable,
        AlbumImageOrientation_Unknown0,
        &entry
    );

    capssuExit();
    free(album_pixels);

    if (R_FAILED(rc)) {
        snprintf(message, message_size,
                 "The Switch Album service rejected the image (0x%08X).", rc);
        return false;
    }

    snprintf(message, message_size,
             "The Switch Album service accepted the image (%ux%u -> %ux%u). "
             "Open Album to verify the entry and selected-game attribution.",
             source_width, source_height, ALBUM_IMAGE_WIDTH, ALBUM_IMAGE_HEIGHT);
    return true;
}

static bool save_video_preflight(const char *media_path,
                                char *message,
                                size_t message_size)
{
    Mp4Info info;
    char probe_error[192] = {0};

    if (!mp4_probe_file(media_path, &info, probe_error, sizeof(probe_error))) {
        snprintf(message, message_size, "MP4 inspection failed: %s", probe_error);
        return false;
    }

    if (!mp4_is_switch_720p_candidate(&info)) {
        snprintf(message, message_size,
                 "MP4 is structurally readable, but it is outside the current "
                 "H.264 1280x720 non-fragmented input profile "
                 "(found %s %ux%u, audio %s). No Album data was written.",
                 mp4_video_codec_name(info.video_codec),
                 info.width, info.height,
                 mp4_audio_codec_name(info.audio_codec));
        return false;
    }

    snprintf(message, message_size,
             "MP4 preflight passed: H.264 1280x720, duration %llu.%03llu s. "
             "The Album movie MakerNote, CMAC and chunk-hash writer are not "
             "implemented yet, so no file was written.",
             (unsigned long long)(info.duration_ms / 1000ULL),
             (unsigned long long)(info.duration_ms % 1000ULL));
    return false;
}

static bool save_selected_media(const char *media_path,
                                const InstalledTitle *title,
                                char *message,
                                size_t message_size)
{
    if (is_mp4_path(media_path))
        return save_video_preflight(media_path, message, message_size);

    return save_image_to_album(media_path, title, message, message_size);
}

static void show_save_result(const InstalledTitle *title,
                             bool succeeded,
                             bool is_video,
                             const char *message)
{
    TimeCalendarTime calendar = {0};
    TimeCalendarAdditionalInfo timezone = {0};
    const bool time_ok = get_current_local_time(&calendar, &timezone);

    consoleClear();
    printf("AlbumSigner - Save result\n");
    printf("=========================\n\n");
    printf("%s\n\n",
           succeeded ? "SYSTEM ACCEPTED THE IMAGE" :
           (is_video ? "VIDEO NOT ADDED TO ALBUM" : "IMAGE WAS NOT SAVED"));
    printf("%s\n\n", message);
    printf("Selected game: %s\n", title->name);
    printf("Title ID: %016llX\n",
           (unsigned long long)title->application_id);

    if (time_ok) {
        printf("Current local time: %04u-%02u-%02u %02u:%02u:%02u\n",
               calendar.year, calendar.month, calendar.day,
               calendar.hour, calendar.minute, calendar.second);
    }

    printf("\nB: choose another image   +: exit\n");
    consoleUpdate(NULL);
}

static void wait_for_exit(void)
{
    PadState pad;
    padInitializeDefault(&pad);

    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus)
            break;
        consoleUpdate(NULL);
    }
}

int albumsigner_run(void)
{
    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    if (appletGetAppletType() != AppletType_Application) {
        consoleClear();
        printf("AlbumSigner requires Application mode.\n\n");
        printf("Relaunch this NRO through a title override.\n");
        printf("Applet mode is not supported.\n\n");
        printf("Press + to exit.\n");
        consoleUpdate(NULL);
        wait_for_exit();
        consoleExit(NULL);
        return 1;
    }

    consoleClear();
    printf("AlbumSigner - Application mode\n");
    printf("===============================\n");
    printf("Mounting SD card...\n");
    consoleUpdate(NULL);

    Result rc = fsdevMountSdmc();
    if (R_FAILED(rc)) {
        printf("Could not mount SD card (0x%08X).\n", rc);
        printf("Press + to exit.\n");
        consoleUpdate(NULL);
        wait_for_exit();
        consoleExit(NULL);
        return 1;
    }

    char media_path[PATH_BUFFER_SIZE];
    if (!browse_for_media(media_path, sizeof(media_path))) {
        fsdevUnmountDevice("sdmc");
        consoleExit(NULL);
        return 0;
    }

    InstalledTitle titles[MAX_TITLES];
    Result title_result = 0;
    const int title_count = load_installed_titles(titles, MAX_TITLES, &title_result);
    if (title_count <= 0) {
        consoleClear();
        printf("Could not load installed games.\n");
        printf("Result: 0x%08X\n\n", title_result);
        printf("Press + to exit.\n");
        consoleUpdate(NULL);
        wait_for_exit();
        fsdevUnmountDevice("sdmc");
        consoleExit(NULL);
        return 1;
    }

    int selected_title = 0;
    if (!select_installed_title(titles, title_count, &selected_title)) {
        fsdevUnmountDevice("sdmc");
        consoleExit(NULL);
        return 0;
    }

    while (appletMainLoop()) {
        show_preview(media_path, &titles[selected_title]);

        PadState pad;
        padInitializeDefault(&pad);
        bool choose_another = false;
        bool add_to_album = false;

        while (appletMainLoop()) {
            padUpdate(&pad);
            const u64 down = padGetButtonsDown(&pad);

            if (down & HidNpadButton_Plus)
                goto cleanup;
            if (down & HidNpadButton_B) {
                choose_another = true;
                break;
            }
            if (down & HidNpadButton_A) {
                add_to_album = true;
                break;
            }
            consoleUpdate(NULL);
        }

        if (add_to_album) {
            char result_message[384] = {0};
            const bool video_input = is_mp4_path(media_path);
            const bool saved = save_selected_media(
                media_path, &titles[selected_title],
                result_message, sizeof(result_message)
            );
            show_save_result(&titles[selected_title], saved, video_input, result_message);

            bool leave_result = false;
            while (appletMainLoop() && !leave_result) {
                padUpdate(&pad);
                const u64 down = padGetButtonsDown(&pad);
                if (down & HidNpadButton_Plus)
                    goto cleanup;
                if (down & HidNpadButton_B)
                    leave_result = true;
                consoleUpdate(NULL);
            }
            choose_another = leave_result;
        }

        if (!choose_another)
            break;

        if (!browse_for_media(media_path, sizeof(media_path)))
            break;
        if (!select_installed_title(titles, title_count, &selected_title))
            break;
    }

cleanup:
    fsdevUnmountDevice("sdmc");
    consoleExit(NULL);
    return 0;
}
