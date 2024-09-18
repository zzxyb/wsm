#ifndef WSM_DESKTOP_H
#define WSM_DESKTOP_H

#include <wayland-server-core.h>

#include <gio/gio.h>
#include <pango/pangocairo.h>

enum wsm_color_scheme {
	Light,
	Dark,
};

struct wsm_desktop_interface {
	struct {
		struct wl_signal icon_theme_change;
		struct wl_signal font_change;
		struct wl_signal cursor_size_change;
		struct wl_signal color_theme_change;
		struct wl_signal destroy;
	} events;

	PangoFontDescription *font_description;
	GSettings *settings;

	char *style_name;
	char *icon_theme;
	char *font_name;
	char *cursor_theme;
	int cursor_size;
	int font_height;
	int font_baseline;
	enum wsm_color_scheme color_scheme;
};

struct wsm_desktop_interface *wsm_desktop_interface_create();
void wsm_desktop_interface_destory(struct wsm_desktop_interface *desktop);
void update_font_height(struct wsm_desktop_interface *desktop);
void set_icon_theme(struct wsm_desktop_interface *desktop, char *icon_theme);
void set_font_name(struct wsm_desktop_interface *desktop, char *font_name);
void set_cursor_size(struct wsm_desktop_interface *desktop, int cursor_size);
void set_color_scheme(struct wsm_desktop_interface *desktop, enum wsm_color_scheme scheme);
void find_app_icon(const char *icon_name, char *icon_path, char *icon_theme, size_t size);
void find_system_icon(const char *icon_name, char *icon_path, char *icon_theme, size_t size);
char* find_app_icon_frome_app_id(struct wsm_desktop_interface *desktop, const char *app_id);
char* find_icon_file_frome_theme(struct wsm_desktop_interface *desktop, const char *icon_name);

#endif
