/*
MIT License

Copyright (c) 2024 YaoBing Xiao

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

    bool is_light;

    struct {
        struct wl_signal icon_theme_change;
        struct wl_signal font_change;
        struct wl_signal cursor_size_change;
        struct wl_signal color_theme_change;
        struct wl_signal destroy;
    } events;
};

struct wsm_desktop_interface *wsm_desktop_interface_create();
void wsm_desktop_interface_destory(struct wsm_desktop_interface *desktop);
void update_font_height(struct wsm_desktop_interface *desktop);
void set_icon_theme(struct wsm_desktop_interface *desktop, char *icon_theme);
void set_font_name(struct wsm_desktop_interface *desktop, char *font_name);
void set_cursor_size(struct wsm_desktop_interface *desktop, int cursor_size);
void set_color_scheme(struct wsm_desktop_interface *desktop, enum wsm_color_scheme scheme);
char* find_app_icon_frome_app_id(struct wsm_desktop_interface *desktop, const char *app_id);

#endif
