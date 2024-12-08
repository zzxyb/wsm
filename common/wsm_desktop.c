#include "wsm_log.h"
#include "wsm_common.h"
#include "wsm_desktop.h"
#include "wsm_pango.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

#define MAX_PATH_LENGTH 2048

#define SYSTEM_ICONS "/usr/share/icons/"
#define SYSTEM_APPLICATIONS "/usr/share/applications/"

struct wsm_desktop_interface *wsm_desktop_interface_create() {
	struct wsm_desktop_interface *desktop =
		calloc(1, sizeof(struct wsm_desktop_interface));
	if (!desktop) {
		wsm_log(WSM_ERROR, "Could not create wsm_desktop_interface: allocation failed!");
		return NULL;
	}

	wl_signal_init(&desktop->events.icon_theme_change);
	wl_signal_init(&desktop->events.font_change);
	wl_signal_init(&desktop->events.cursor_size_change);
	wl_signal_init(&desktop->events.color_theme_change);
	wl_signal_init(&desktop->events.style_theme_change);
	wl_signal_init(&desktop->events.cursor_theme_change);
	wl_signal_init(&desktop->events.destroy);

	// TODO: use config
	set_style_name(desktop, "Breeze");
	set_icon_theme(desktop, "breeze");
	set_font_name(desktop, "Noto Sans 10");
	set_color_scheme(desktop, Light);
	set_cursor_theme(desktop, "breeze_cursors");
	set_cursor_size(desktop, 24);

	return desktop;
}

void wsm_desktop_interface_destory(struct wsm_desktop_interface *desktop) {
	wl_signal_emit_mutable(&desktop->events.destroy, desktop);

	pango_font_description_free(desktop->font_description);
	free(desktop->style_name);
	free(desktop->icon_theme);
	free(desktop->font_name);
	free(desktop->cursor_theme);
	free(desktop);
}

void update_font_height(struct wsm_desktop_interface *desktop) {
	get_text_metrics(desktop->font_description, &desktop->font_height, &desktop->font_baseline);
}

void set_style_name(struct wsm_desktop_interface *desktop, char *style_name) {
	if (!style_name) {
		return;
	}

	if (desktop->style_name && strcmp(desktop->style_name, style_name) == 0) {
		return;
	}

	free(desktop->style_name);
	desktop->style_name = strdup(style_name);
	wl_signal_emit_mutable(&desktop->events.style_theme_change, desktop);
}

void set_cursor_theme(struct wsm_desktop_interface *desktop, char *cursor_theme) {
	if (!cursor_theme) {
		return;
	}

	if (desktop->cursor_theme && strcmp(desktop->cursor_theme, cursor_theme) == 0) {
		return;
	}

	free(desktop->cursor_theme);
	desktop->cursor_theme = strdup(cursor_theme);
	wl_signal_emit_mutable(&desktop->events.cursor_theme_change, desktop);
}

void set_icon_theme(struct wsm_desktop_interface *desktop, char *icon_theme) {
	if (!icon_theme) {
		return;
	}

	if (desktop->icon_theme && strcmp(desktop->icon_theme, icon_theme) == 0) {
		return;
	}

	free(desktop->icon_theme);
	desktop->icon_theme = strdup(icon_theme);
	wl_signal_emit_mutable(&desktop->events.icon_theme_change, desktop);
}

void set_font_name(struct wsm_desktop_interface *desktop, char *font_name) {
	if (!font_name) {
		return;
	}

	if (desktop->font_name && strcmp(desktop->font_name, font_name) == 0) {
		return;
	}

	free(desktop->font_name);
	desktop->font_name = strdup(font_name);

	PangoFontDescription *font_description =
		pango_font_description_from_string(desktop->font_name);

	const char *family = pango_font_description_get_family(font_description);
	if (family == NULL) {
		pango_font_description_free(font_description);
		wsm_log(WSM_ERROR, "Invalid font family.");
		return;
	}

	const gint size = pango_font_description_get_size(font_description);
	if (size == 0) {
		pango_font_description_free(font_description);
		wsm_log(WSM_ERROR, "Invalid font size.");
	}

	desktop->font_description = font_description;
	update_font_height(desktop);
	wl_signal_emit_mutable(&desktop->events.font_change, desktop);
}

void set_cursor_size(struct wsm_desktop_interface *desktop, int cursor_size) {
	if (desktop->cursor_size == cursor_size) {
		return;
	}

	desktop->cursor_size = cursor_size;
	wl_signal_emit_mutable(&desktop->events.cursor_size_change, desktop);
}

void set_color_scheme(struct wsm_desktop_interface *desktop,
		enum wsm_color_scheme scheme) {
	if (scheme == desktop->color_scheme) {
		return;
	}

	desktop->color_scheme = scheme;
	wl_signal_emit_mutable(&desktop->events.color_theme_change, desktop);
}

static char* find_desktop_file_frome_app_id(const char *identifier) {
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

static char* get_icon_name_from_desktop_file(const char* desktop_file_path) {
	FILE *file = fopen(desktop_file_path, "r");
	if (!file) {
		perror("Failed to open desktop file");
		
		return NULL;
	}

	char buffer[1024];
	char *icon_name = NULL;
	while (fgets(buffer, sizeof(buffer), file)) {
		if (strncmp(buffer, "Icon=", 5) == 0) {
			icon_name = strdup(buffer + 5);
			icon_name[strcspn(icon_name, "\n")] = '\0';
			break;
		}
	}
	fclose(file);

	return icon_name;
}

static void get_home_directory(char *home_dir, size_t size) {
	char *home = getenv("HOME");
	if (home) {
		strncpy(home_dir, home, size - 1);
	} else {
		wsm_log(WSM_ERROR, "Could not get home directory");
		exit(EXIT_FAILURE);
	}
}

static int file_exists(const char *path) {
	struct stat buffer;
	return (stat(path, &buffer) == 0);
}

static void find_icon_in_directory(const char *directory, const char *icon_name,
		const char *icon_extensions[],
		char *icon_path, size_t size) {
	for (size_t i = 0; icon_extensions[i] != NULL; ++i) {
		snprintf(icon_path, size, "%s/%s.%s",
				 directory, icon_name, icon_extensions[i]);
		if (file_exists(icon_path)) {
			return;
		}
	}

	icon_path[0] = '\0';
}

// starts at 64x64
// "16x16",
// "24x24",
// "32x32",
// "48x48",
// starts at 64
// "16",
//"24",
//"32",
//"48",
static const char *icon_sizes[] = {
	"64x64",
	"128x128",
	"256x256",
	"scalable",
	"64",
	"128",
	"256",
	NULL
};

// "16",
//"24",
//"32",
//"64",
static const char *systemd_icon_sizes[] = {
	"48",
	NULL
};

static const char *system_icon_extensions[] = {"svg", NULL};
static const char *icon_extensions[] = {"png", "svg", "xpm", NULL};

void find_app_icon(const char *icon_name, char *icon_path,
		char *icon_theme, size_t size) {
	char home_dir[256];
	get_home_directory(home_dir, sizeof(home_dir));

	const char *icon_dirs[] = {
		"/usr/share/icons",
		"/usr/local/share/icons",
		"/usr/share/pixmaps",
		home_dir,
		NULL
	};

	const char *icon_subdirs[] = {
		"hicolor",
		icon_theme,
		NULL
	};

	for (size_t i = 0; icon_dirs[i] != NULL; ++i) {
		char directory[512];
		for (size_t j = 0; icon_subdirs[j] != NULL; ++j) {
			for (size_t k = 0; icon_sizes[k] != NULL; ++k) {
				snprintf(directory, sizeof(directory), "%s/%s/%s/apps",
					icon_dirs[i], icon_subdirs[j], icon_sizes[k]);
				find_icon_in_directory(directory, icon_name,
					icon_extensions, icon_path, size);
				if (icon_path[0] != '\0') {
					return;
				}

				snprintf(directory, sizeof(directory), "%s/%s/apps/%s",
					icon_dirs[i], icon_subdirs[j], icon_sizes[k]);
				find_icon_in_directory(directory, icon_name,
					icon_extensions, icon_path, size);
				if (icon_path[0] != '\0') {
					return;
				}
			}

			snprintf(directory, sizeof(directory), "%s/%s",
				icon_dirs[i], icon_subdirs[j]);
			find_icon_in_directory(directory, icon_name,
				icon_extensions, icon_path, size);
			if (icon_path[0] != '\0') {
				return;
			}
		}

		snprintf(directory, sizeof(directory), "%s/pixmaps",
			icon_dirs[i]);
		find_icon_in_directory(directory, icon_name,
			icon_extensions, icon_path, size);
		if (icon_path[0] != '\0') {
			return;
		}
	}
}

void find_system_icon(const char *icon_name, char *icon_path, char *icon_theme, size_t size) {
	const char *icon_dirs[] = {
		"/usr/share/icons",
		NULL
	};

	const char *icon_subdirs[] = {
		icon_theme,
		NULL
	};

	for (size_t i = 0; icon_dirs[i] != NULL; ++i) {
		char directory[512];
		for (size_t j = 0; icon_subdirs[j] != NULL; ++j) {
			for (size_t k = 0; systemd_icon_sizes[k] != NULL; ++k) {
				snprintf(directory, sizeof(directory), "%s/%s/apps/%s",
					icon_dirs[i], icon_subdirs[j], systemd_icon_sizes[k]);

				find_icon_in_directory(directory, icon_name, system_icon_extensions, icon_path, size);
				if (icon_path[0] != '\0') {
					return;
				}
			}
		}
	}
}

char* find_app_icon_frome_app_id(struct wsm_desktop_interface *desktop, const char *app_id) {
	char *desktop_file_path = find_desktop_file_frome_app_id(app_id);
	if (!desktop_file_path) {
		printf("Error: .desktop file not found for identifier: %s\n", app_id);

		return NULL;
	}

	char *icon_name = get_icon_name_from_desktop_file(desktop_file_path);
	char icon_path[MAX_PATH_LENGTH] = "";
	find_app_icon(icon_name, icon_path, desktop->icon_theme, sizeof(icon_path));
	return strdup(icon_path);
}

char* find_icon_file_frome_theme(struct wsm_desktop_interface *desktop, const char *icon_name) {
	char icon_path[MAX_PATH_LENGTH] = "";
	find_system_icon(icon_name, icon_path, desktop->icon_theme, sizeof(icon_path));
	return strdup(icon_path);
}
