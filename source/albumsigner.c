#include <switch.h>

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "albumsigner.h"

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
           strcasecmp(dot, ".mp4") == 0 ||
           strcasecmp(dot, ".mov") == 0 ||
           strcasecmp(dot, ".m4v") == 0;
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
            printf("Inputs: JPG, JPEG, PNG, MP4, MOV, M4V\n");
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

    consoleClear();
    printf("AlbumSigner - Preview\n");
    printf("=====================\n\n");
    printf("Selected media:\n%s\n\n", media_path);
    printf("Selected game:\n%s\n", title->name);
    printf("Application Title ID: %016llX\n\n",
           (unsigned long long)title->application_id);

    if (time_ok) {
        printf("Capture time (console local time):\n");
        printf("%04u-%02u-%02u %02u:%02u:%02u\n",
               calendar.year, calendar.month, calendar.day,
               calendar.hour, calendar.minute, calendar.second);
        printf("Timezone: %s (UTC offset %d seconds)\n\n",
               timezone.timezoneName, timezone.offset);
        printf("Planned Album folder:\n");
        printf("sdmc:/Nintendo/Album/%04u/%02u/%02u/\n\n",
               calendar.year, calendar.month, calendar.day);
    } else {
        printf("Could not read the console's local time.\n\n");
    }

    printf("STATUS: Selection preview only.\n");
    printf("No file has been created or changed.\n");
    printf("Album metadata and authentication are not implemented yet.\n\n");
    printf("B: choose another file   +: exit\n");
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

        while (appletMainLoop()) {
            padUpdate(&pad);
            const u64 down = padGetButtonsDown(&pad);

            if (down & HidNpadButton_Plus)
                goto cleanup;
            if (down & HidNpadButton_B) {
                choose_another = true;
                break;
            }
            consoleUpdate(NULL);
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
