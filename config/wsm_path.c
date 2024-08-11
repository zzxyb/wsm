/*
MIT License

Copyright (c) 2024 WenHao Peng

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "wsm_path.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <wayland-util.h>

// theme operate
struct wsm_dir {
    const char *prefix;
    const char *default_prefix;
    const char *path;
};

static struct wsm_dir theme_dirs[] = {
    {
        .prefix = "XDG_DATA_HOME",
        .default_prefix = "$HOME/.local/share",
        .path = "themes",
    }, {
        .prefix = "HOME",
        .path = ".themes",
    }, {
        .prefix = "XDG_DATA_DIRS",
        .default_prefix = "/usr/share:/usr/local/share",
        .path = "themes",
    }, {
        .path = NULL,
    }
};

void expand_shell_variables(char *str) {
    char *home = getenv("HOME");
    if (home) {
        char *pos = strstr(str, "$HOME");
        if (pos) {
            size_t home_len = strlen(home);
            size_t len = strlen(pos + 5);
            memmove(pos + home_len, pos + 5, len + 1); // Move the rest of the string
            memcpy(pos, home, home_len); // Copy home directory into the string
        }
    }
}

int path_exists(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

void find_theme_paths(const char *theme_name, struct wl_list *theme_list) {
    for (int i = 0; theme_dirs[i].path != NULL; i++) {
        struct wsm_dir *d = &theme_dirs[i];

        const char *prefix = getenv(d->prefix);
        if (!prefix && d->default_prefix) {
            prefix = d->default_prefix;
        }

        struct wl_array prefixes;
        wl_array_init(&prefixes);
        char *prefixes_str = strdup(prefix);
        char *token = strtok(prefixes_str, ":");
        while (token) {
            char *p = wl_array_add(&prefixes, strlen(token) + 1);
            strcpy(p, token);
            token = strtok(NULL, ":");
        }
        free(prefixes_str);

        char *p;
        wl_array_for_each(p, &prefixes) {
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s/%s", p, d->path, theme_name);

            expand_shell_variables(full_path);

            if (path_exists(full_path)) {
                struct wsm_theme_info *theme = malloc(sizeof(struct wsm_theme_info));
                if (theme) {
                    theme->path = strdup(full_path);
                    wl_list_insert(theme_list, &theme->link);
                }
            }
        }
        wl_array_release(&prefixes);
    }
}

void print_theme_paths(const struct wl_list *theme_list) {
    struct wsm_theme_info *theme;
    wl_list_for_each(theme, (struct wl_list *)theme_list, link) {
        printf("Theme found at: %s\n", theme->path);
    }
}

void free_theme_list(struct wl_list *theme_list) {
    struct wsm_theme_info *theme, *tmp;

    wl_list_for_each_safe(theme, tmp, theme_list, link) {
        wl_list_remove(&theme->link);
        free(theme->path);
        free(theme);
    }
}

// icon operate
#define MAX_LINE_LENGTH 256
#define MAX_PATH_LENGTH 1024

#define SYSTEM_APPLICATIONS "/usr/share/applications/"
#define SYSTEM_ICONS "/usr/share/icons/"
#define SYSTEM_ICON_THEME "la-capitaine-icon-theme"

char* find_desktop_file_path(const char *identifier) {
    char filename[256];
    snprintf(filename, sizeof(filename), "%s.desktop", identifier);

    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s%s", SYSTEM_APPLICATIONS, filename);

    struct stat st;
    if (stat(full_path, &st) == 0) {
        return strdup(full_path);
    }

    return NULL;
}

// Function to parse .desktop file and find the Icon field
char* parse_desktop_file_for_icon(const char *identifier) {
    // Find the full path of the .desktop file
    char *desktop_file_path = find_desktop_file_path(identifier);
    if (!desktop_file_path) {
        printf("Error: .desktop file not found for identifier: %s\n", identifier);
        return NULL;
    }

    FILE *file = fopen(desktop_file_path, "r");
    if (!file) {
        perror("Error opening .desktop file");
        free(desktop_file_path);
        return NULL;
    }

    char line[MAX_LINE_LENGTH];
    char *icon_name = NULL;

    while (fgets(line, sizeof(line), file)) {
        // Look for the line that starts with "Icon="
        if (strncmp(line, "Icon=", 5) == 0) {
            // Strip newline character and copy the icon name
            line[strcspn(line, "\n")] = '\0';
            icon_name = strdup(line + 5); // Copy everything after "Icon="
            break;
        }
    }

    fclose(file);
    free(desktop_file_path);
    return icon_name;
}

// Function to find the full path of the icon in a given theme
char* find_icon_path(const char *theme_name, const char *icon_name) {
    char path[MAX_PATH_LENGTH];

    snprintf(path, sizeof(path), "%s%s/apps/scalable/%s.png", SYSTEM_ICONS, theme_name, icon_name);

    struct stat st;
    if (stat(path, &st) == 0) {
        return strdup(path);
    }

    // maybe .svg icon
    snprintf(path, sizeof(path), "%s%s/apps/scalable/%s.svg", SYSTEM_ICONS, theme_name, icon_name);

    if (stat(path, &st) == 0) {
        return strdup(path);
    }
    return NULL;
}

char* find_theme_identifier_path(const char *identifier) {
    char *icon_name = parse_desktop_file_for_icon(identifier);

    if (icon_name == NULL) {
        return NULL;
    }

    char *path = find_icon_path(SYSTEM_ICON_THEME, icon_name);

    struct stat st;
    if (stat(path, &st) == 0) {
        return strdup(path);
    }

    return NULL;
}

