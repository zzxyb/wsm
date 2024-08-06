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

#ifndef WSM_VIEW_H
#define WSM_VIEW_H

#include "../config.h"

#include <wayland-server-core.h>

#include <wlr/types/wlr_compositor.h>

struct wlr_scene_node;
struct wlr_scene_tree;
struct wlr_xdg_surface;
struct wlr_foreign_toplevel_handle_v1;

struct wsm_seat;
struct wsm_view;
struct wsm_output;
struct wsm_container;
struct wsm_workspace;
struct wsm_xdg_decoration;

enum wsm_view_type {
    WSM_VIEW_XDG_SHELL,
#if HAVE_XWAYLAND
    WSM_VIEW_XWAYLAND,
#endif
};

enum wsm_view_prop {
    VIEW_PROP_TITLE,
    VIEW_PROP_APP_ID,
    VIEW_PROP_CLASS,
    VIEW_PROP_INSTANCE,
    VIEW_PROP_WINDOW_TYPE,
    VIEW_PROP_WINDOW_ROLE,
#if HAVE_XWAYLAND
    VIEW_PROP_X11_WINDOW_ID,
    VIEW_PROP_X11_PARENT_ID,
#endif
};

struct wsm_view_impl {
    void (*get_constraints)(struct wsm_view *view, double *min_width,
                            double *max_width, double *min_height, double *max_height);
    const char *(*get_string_prop)(struct wsm_view *view,
                                   enum wsm_view_prop prop);
    uint32_t (*get_int_prop)(struct wsm_view *view, enum wsm_view_prop prop);
    uint32_t (*configure)(struct wsm_view *view, double lx, double ly,
                          int width, int height);
    void (*set_activated)(struct wsm_view *view, bool activated);
    void (*set_tiled)(struct wsm_view *view, bool tiled);
    void (*set_fullscreen)(struct wsm_view *view, bool fullscreen);
    void (*set_resizing)(struct wsm_view *view, bool resizing);
    bool (*wants_floating)(struct wsm_view *view);
    bool (*is_transient_for)(struct wsm_view *child,
                             struct wsm_view *ancestor);
    void (*maximize)(struct wsm_view *view, bool maximize);
    void (*minimize)(struct wsm_view *view, bool minimize);
    void (*close)(struct wsm_view *view);
    void (*close_popups)(struct wsm_view *view);
    void (*destroy)(struct wsm_view *view);
};

struct wsm_view {
    enum wsm_view_type type;
    const struct wsm_view_impl *impl;

    struct wlr_scene_tree *scene_tree;
    struct wlr_scene_tree *content_tree;
    struct wlr_scene_tree *saved_surface_tree;

    struct wsm_container *container; // NULL if unmapped and transactions finished
    struct wlr_surface *surface; // NULL for unmapped views
    struct wsm_xdg_decoration *xdg_decoration;

    pid_t pid;

    // The size the view would want to be if it weren't tiled.
    // Used when changing a view from tiled to floating.
    int natural_width, natural_height;

    char *title_format;

    bool using_csd;

    struct timespec urgent;
    bool allow_request_urgent;
    struct wl_event_source *urgent_timer;

    // The geometry for whatever the client is committing, regardless of
    // transaction state. Updated on every commit.
    struct wlr_box geometry;

    struct wlr_ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel;

    struct wlr_foreign_toplevel_handle_v1 *foreign_toplevel;
    struct wl_listener foreign_activate_request;
    struct wl_listener foreign_fullscreen_request;
    struct wl_listener foreign_close_request;
    struct wl_listener foreign_destroy;

    bool destroying;

    union {
        struct wlr_xdg_toplevel *wlr_xdg_toplevel;
#if HAVE_XWAYLAND
        struct wlr_xwayland_surface *wlr_xwayland_surface;
#endif
    };

    struct {
        struct wl_signal unmap;
    } events;

    int max_render_time; // In milliseconds
    bool enabled;
};

struct wsm_xdg_shell_view {
    struct wsm_view view;

    struct wl_listener commit;
    struct wl_listener request_move;
    struct wl_listener request_resize;
    struct wl_listener request_maximize;
    struct wl_listener request_fullscreen;
    struct wl_listener set_title;
    struct wl_listener set_app_id;
    struct wl_listener new_popup;
    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener destroy;
};
#if HAVE_XWAYLAND
struct wsm_xwayland_view {
    struct wsm_view view;

    struct wlr_scene_tree *surface_tree;

    struct wl_listener commit;
    struct wl_listener request_move;
    struct wl_listener request_resize;
    struct wl_listener request_maximize;
    struct wl_listener request_minimize;
    struct wl_listener request_configure;
    struct wl_listener request_fullscreen;
    struct wl_listener request_activate;
    struct wl_listener set_title;
    struct wl_listener set_class;
    struct wl_listener set_role;
    struct wl_listener set_startup_id;
    struct wl_listener set_window_type;
    struct wl_listener set_hints;
    struct wl_listener set_decorations;
    struct wl_listener associate;
    struct wl_listener dissociate;
    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener destroy;
    struct wl_listener override_redirect;

    struct wl_listener surface_tree_destroy;
};

struct wsm_xwayland_unmanaged {
    struct wlr_xwayland_surface *wlr_xwayland_surface;

    struct wlr_scene_surface *surface_scene;

    struct wl_listener request_activate;
    struct wl_listener request_configure;
    struct wl_listener request_fullscreen;
    struct wl_listener set_geometry;
    struct wl_listener associate;
    struct wl_listener dissociate;
    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener destroy;
    struct wl_listener override_redirect;
};
#endif

struct wsm_popup_desc {
    struct wlr_scene_node *relative;
    struct wsm_view *view;
};

bool view_init(struct wsm_view *view, enum wsm_view_type type,
               const struct wsm_view_impl *impl);
void view_destroy(struct wsm_view *view);
void view_begin_destroy(struct wsm_view *view);
const char *view_get_title(struct wsm_view *view);
const char *view_get_app_id(struct wsm_view *view);
const char *view_get_class(struct wsm_view *view);
const char *view_get_instance(struct wsm_view *view);
uint32_t view_get_x11_window_id(struct wsm_view *view);
uint32_t view_get_x11_parent_id(struct wsm_view *view);
const char *view_get_window_role(struct wsm_view *view);
uint32_t view_get_window_type(struct wsm_view *view);
const char *view_get_shell(struct wsm_view *view);
void view_get_constraints(struct wsm_view *view, double *min_width,
                          double *max_width, double *min_height, double *max_height);
uint32_t view_configure(struct wsm_view *view, double lx, double ly, int width,
                        int height);
bool view_inhibit_idle(struct wsm_view *view);
void view_autoconfigure(struct wsm_view *view);
void view_set_activated(struct wsm_view *view, bool activated);
void view_request_activate(struct wsm_view *view, struct wsm_seat *seat);
void view_request_urgent(struct wsm_view *view);
void view_set_csd_from_server(struct wsm_view *view, bool enabled);
void view_update_csd_from_client(struct wsm_view *view, bool enabled);
void view_set_tiled(struct wsm_view *view, bool tiled);
void view_maximize(struct wsm_view *view, bool maximize);
void view_minimize(struct wsm_view *view, bool minimize);
void view_close(struct wsm_view *view);
void view_close_popups(struct wsm_view *view);
void view_set_enable(struct wsm_view *view, bool enable);
void view_map(struct wsm_view *view, struct wlr_surface *wlr_surface,
              bool fullscreen, struct wlr_output *fullscreen_output, bool decoration);
void view_unmap(struct wsm_view *view);
void view_update_size(struct wsm_view *view);
void view_center_and_clip_surface(struct wsm_view *view);
struct wsm_view *view_from_wlr_surface(struct wlr_surface *surface);
void view_update_app_id(struct wsm_view *view);
void view_update_title(struct wsm_view *view, bool force);
bool view_is_visible(struct wsm_view *view);
void view_set_urgent(struct wsm_view *view, bool enable);
bool view_is_urgent(struct wsm_view *view);
void view_remove_saved_buffer(struct wsm_view *view);
void view_save_buffer(struct wsm_view *view);
bool view_is_transient_for(struct wsm_view *child, struct wsm_view *ancestor);
void view_send_frame_done(struct wsm_view *view);

#endif
