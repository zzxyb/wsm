/**
 * @file        wsm_desktop.h
 * @brief       Desktop integration helpers for wlframe.
 * @details     This file defines the desktop interface state used for icon
 *              themes, font settings, cursor settings, color scheme tracking,
 *              and desktop file based application icon/name lookup.
 * @author      YaoBing Xiao
 * @date        2026-07-21
 * @version     v1.0
 * @par Copyright(c):
 * @par History:
 *      version: v1.0, YaoBing Xiao, 2026-07-21, initial version\n
 */

#ifndef UTIL_WSM_DESKTOP_H
#define UTIL_WSM_DESKTOP_H

#include <wayland-server-core.h>

#include <pango/pangocairo.h>

/**
 * @brief Enumeration of color schemes for the desktop interface.
 */
enum wsm_color_scheme {
	Light, /**< Light color scheme */
	Dark, /**< Dark color scheme */
};

/**
 * @brief Structure representing the desktop interface.
 */
struct wsm_desktop_interface {
	struct {
		struct wl_signal icon_theme_change; /**< Signal emitted when the icon theme changes */
		struct wl_signal font_change; /**< Signal emitted when the font changes */
		struct wl_signal cursor_size_change; /**< Signal emitted when the cursor size changes */
		struct wl_signal color_theme_change; /**< Signal emitted when the color theme changes */
		struct wl_signal style_theme_change; /**< Signal emitted when the style theme changes */
		struct wl_signal cursor_theme_change; /**< Signal emitted when the cursor theme changes */
		struct wl_signal destroy; /**< Signal emitted when the desktop interface is destroyed */
	} events; /**< Events associated with the desktop interface */

	PangoFontDescription *font_description; /**< Font description for the desktop interface */

	char *style_name; /**< Name of the style */
	char *icon_theme; /**< Current icon theme */
	char *font_name; /**< Current font name */
	char *cursor_theme; /**< Current cursor theme */
	int cursor_size; /**< Size of the cursor */
	int font_height; /**< Height of the font */
	int font_baseline; /**< Baseline of the font */
	enum wsm_color_scheme color_scheme; /**< Current color scheme */
};

/**
 * @brief Creates a new desktop interface instance.
 * @return Pointer to the newly created wsm_desktop_interface instance.
 */
struct wsm_desktop_interface *wsm_desktop_interface_create();

/**
 * @brief Refreshes desktop settings from the current system configuration.
 * @param desktop Pointer to the wsm_desktop_interface instance.
 */
void wsm_desktop_interface_refresh_system_settings(
	struct wsm_desktop_interface *desktop);

/**
 * @brief Destroys the specified desktop interface instance.
 * @param desktop Pointer to the wsm_desktop_interface instance to destroy.
 */
void wsm_desktop_interface_destory(struct wsm_desktop_interface *desktop);

/**
 * @brief Updates the font height in the desktop interface.
 * @param desktop Pointer to the wsm_desktop_interface instance.
 */
void update_font_height(struct wsm_desktop_interface *desktop);

/**
 * @brief Sets the style name for the desktop interface.
 * @param desktop Pointer to the wsm_desktop_interface instance.
 * @param style_name Pointer to the new style name string.
 */
void set_style_name(struct wsm_desktop_interface *desktop, char *style_name);

/**
 * @brief Sets the cursor theme for the desktop interface.
 * @param desktop Pointer to the wsm_desktop_interface instance.
 * @param cursor_theme Pointer to the new cursor theme string.
 */
void set_cursor_theme(struct wsm_desktop_interface *desktop, char *cursor_theme);

/**
 * @brief Sets the icon theme for the desktop interface.
 * @param desktop Pointer to the wsm_desktop_interface instance.
 * @param icon_theme Pointer to the new icon theme string.
 */
void set_icon_theme(struct wsm_desktop_interface *desktop, char *icon_theme);

/**
 * @brief Sets the font name for the desktop interface.
 * @param desktop Pointer to the wsm_desktop_interface instance.
 * @param font_name Pointer to the new font name string.
 */
void set_font_name(struct wsm_desktop_interface *desktop, char *font_name);

/**
 * @brief Sets the cursor size for the desktop interface.
 * @param desktop Pointer to the wsm_desktop_interface instance.
 * @param cursor_size The new cursor size.
 */
void set_cursor_size(struct wsm_desktop_interface *desktop, int cursor_size);

/**
 * @brief Sets the color scheme for the desktop interface.
 * @param desktop Pointer to the wsm_desktop_interface instance.
 * @param scheme The new color scheme to set.
 */
void set_color_scheme(struct wsm_desktop_interface *desktop, enum wsm_color_scheme scheme);

/**
 * @brief Finds the application icon based on the icon name.
 * @param icon_name The name of the icon to find.
 * @param icon_path Pointer to store the path of the found icon.
 * @param icon_theme Pointer to store the theme of the found icon.
 * @param size Size of the icon path buffer.
 */
void find_app_icon(const char *icon_name, char *icon_path, char *icon_theme, size_t size);

/**
 * @brief Finds the system icon based on the icon name.
 * @param icon_name The name of the icon to find.
 * @param icon_path Pointer to store the path of the found icon.
 * @param icon_theme Pointer to store the theme of the found icon.
 * @param size Size of the icon path buffer.
 */
void find_system_icon(const char *icon_name, char *icon_path, char *icon_theme, size_t size);

/**
 * @brief Finds the application icon based on the application ID.
 * @param desktop Pointer to the wsm_desktop_interface instance.
 * @param app_id The application ID to find the icon for.
 * @return Pointer to the found icon path string.
 */
char* find_app_icon_frome_app_id(struct wsm_desktop_interface *desktop, const char *app_id);

/**
 * @brief Finds the display name from an application's desktop file.
 * @param app_id Application identifier used to locate the desktop file.
 * @return Newly allocated display name, or NULL when none can be found.
 */
char *find_app_name_from_app_id(const char *app_id);

/**
 * @brief Finds the icon file based on the theme and icon name.
 * @param desktop Pointer to the wsm_desktop_interface instance.
 * @param icon_name The name of the icon to find.
 * @return Pointer to the found icon file path string.
 */
char* find_icon_file_frome_theme(struct wsm_desktop_interface *desktop, const char *icon_name);

#endif // UTIL_WSM_DESKTOP_H
