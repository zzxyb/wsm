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

#include <wlr/xwayland/xwayland.h>
#include <wlr/types/wlr_compositor.h>

struct wlr_scene_node;
struct wlr_scene_tree;
struct wlr_xdg_surface;
struct wlr_foreign_toplevel_handle_v1;

struct wsm_output;
struct wsm_container;
struct wsm_workspace;
struct wsm_xdg_decoration;

enum wsm_view_type {
    WSM_VIEW_XDG_SHELL,
    WSM_VIEW_LAYER_SHELL,
#ifdef HAVE_XWAYLAND
    WSM_VIEW_XWAYLAND,
#endif
};

enum view_axis {
    VIEW_AXIS_NONE = 0,
    VIEW_AXIS_HORIZONTAL = (1 << 0),
    VIEW_AXIS_VERTICAL = (1 << 1),
    VIEW_AXIS_BOTH = (VIEW_AXIS_HORIZONTAL | VIEW_AXIS_VERTICAL),
};

enum view_edge {
    VIEW_EDGE_INVALID = 0,

    VIEW_EDGE_LEFT,
    VIEW_EDGE_RIGHT,
    VIEW_EDGE_UP,
    VIEW_EDGE_DOWN,
    VIEW_EDGE_CENTER,
};

enum view_wants_focus {
    VIEW_WANTS_FOCUS_NEVER = 0,
    VIEW_WANTS_FOCUS_ALWAYS,
    VIEW_WANTS_FOCUS_OFFER,
};

enum wsm_view_prop {
    VIEW_PROP_TITLE,
    VIEW_PROP_APP_ID,
    VIEW_PROP_CLASS,
    VIEW_PROP_INSTANCE,
    VIEW_PROP_WINDOW_TYPE,
    VIEW_PROP_WINDOW_ROLE,
#ifdef HAVE_XWAYLAND
    VIEW_PROP_X11_WINDOW_ID,
    VIEW_PROP_X11_PARENT_ID,
#endif
};

struct wsm_view_size_hints {
    int min_width;
    int min_height;
    int width_inc;
    int height_inc;
    int base_width;
    int base_height;
};

struct wsm_view_impl;

struct wsm_view {
    enum wsm_view_type type;
    const struct wsm_view_impl *impl;

    struct wsm_container *container;
    struct wlr_surface *surface;
    struct wsm_xdg_decoration *xdg_decoration;
    struct wl_list link;
    struct wsm_output *output;
    int natural_width, natural_height;

    void *data;
    struct wlr_scene_tree *scene_tree;
    struct wsm_workspace *workspace;

    bool mapped;
    bool fullscreen;
    pid_t pid;
    char *title_format;
    enum view_edge tiled;
    /* Pointer to an output owned struct region, may be NULL */
    struct region *tiled_region;
    /* Set to region->name when tiled_region is free'd by a destroying output */
    char *tiled_region_evacuate;
    enum view_axis maximized;

    struct wlr_box current;
    struct wlr_box pending;

    bool destroying;

    uint32_t pending_configure_serial;
    struct wl_event_source *pending_configure_timeout;

    union {
        struct wlr_xdg_toplevel *wlr_xdg_toplevel;
#ifdef HAVE_XWAYLAND
        struct wlr_xwayland_surface *wlr_xwayland_surface;
#endif
    };

    struct {
        struct wl_signal unmap;
    } events;

    struct foreign_toplevel {
        struct wlr_foreign_toplevel_handle_v1 *handle;
        struct wl_listener maximize;
        struct wl_listener minimize;
        struct wl_listener fullscreen;
        struct wl_listener activate;
        struct wl_listener close;
        struct wl_listener destroy;
    } toplevel;

    struct wl_listener surface_new_subsurface;
};

struct wsm_view_impl {
    void (*configure)(struct wsm_view *view, struct wlr_box geo);
    void (*close)(struct wsm_view *view);
    void (*close_popups)(struct wsm_view *surface);
    void (*destroy)(struct wsm_view *surface);
    const char *(*get_string_prop)(struct wsm_view *surface,
                                   enum wsm_view_prop prop);
    uint32_t (*get_int_prop)(struct wsm_view *surface, enum wsm_view_prop prop);
    void (*map)(struct wsm_view *view);
    void (*set_activated)(struct wsm_view *view, bool activated);
    void (*set_fullscreen)(struct wsm_view *view, bool fullscreen);
    void (*set_resizing)(struct wsm_view *surface, bool resizing);
    void (*set_tiled)(struct wsm_view *view, bool tiled);
    void (*unmap)(struct wsm_view *view, bool client_request);
    void (*maximize)(struct wsm_view *view, bool maximize);
    void (*minimize)(struct wsm_view *view, bool minimize);
    void (*move_to_front)(struct wsm_view *view);
    void (*move_to_back)(struct wsm_view *view);
    struct wsm_view *(*get_root)(struct wsm_view *self);
    void (*append_children)(struct wsm_view *self, struct wl_array *children);
    struct wsm_view_size_hints (*get_size_hints)(struct wsm_view *self);
    enum view_wants_focus (*wants_focus)(struct wsm_view *self);
    bool (*has_strut_partial)(struct wsm_view *self);
    bool (*contains_window_type)(struct wsm_view *view, int32_t window_type);
    pid_t (*get_pid)(struct wsm_view *view);
};

struct wsm_wayland_view {
    struct wsm_view surface;

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

struct wsm_view_child {
    const struct wsm_view_child_impl *impl;
    struct wl_list link;

    struct wsm_view *view;
    struct wsm_view_child *parent;
    struct wl_list children;
    struct wlr_surface *surface;
    bool mapped;

    struct wl_listener surface_commit;
    struct wl_listener surface_new_subsurface;
    struct wl_listener surface_map;
    struct wl_listener surface_unmap;
    struct wl_listener surface_destroy;
    struct wl_listener view_unmap;
};

struct wsm_view_child_impl {
    void (*get_view_coords)(struct wsm_view_child *child, int *sx, int *sy);
    void (*destroy)(struct wsm_view_child *child);
};

struct wsm_xdg_shell_view {
    struct wsm_view view;
    struct wlr_xdg_surface *xdg_surface;

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

struct wsm_xdg_toplevel_view {
    struct wsm_view base;
    struct wlr_xdg_surface *xdg_surface;

    struct wl_listener set_app_id;
    struct wl_listener new_popup;
};

#ifdef HAVE_XWAYLAND
struct wsm_xwayland_view {
    struct wsm_view view;
    struct wlr_xwayland_surface *xwayland_surface;

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
};

#endif

void wsm_view_init(struct wsm_view *view, enum wsm_view_type type,
               const struct wsm_view_impl *impl);
void wsm_view_destroy(struct wsm_view *view);

void wsm_view_update_outputs(struct wsm_view *view);
void wsm_view_update_title(struct wsm_view *view);
void wsm_view_update_app_id(struct wsm_view *view);
void surface_update_outputs(struct wlr_surface *surface);
void surface_enter_output(struct wlr_surface *surface,
                          struct wsm_output *output);
void surface_leave_output(struct wlr_surface *surface,
                          struct wsm_output *output);

void wsm_view_set_activated(struct wsm_view *view, bool activated);
void wsm_view_set_output(struct wsm_view *view, struct wsm_output *output);
void wsm_view_close(struct wsm_view *view);
void wsm_view_move_resize(struct wsm_view *view, struct wlr_box geo);
void wsm_view_resize_relative(struct wsm_view *view,
                          int left, int right, int top, int bottom);
void wsm_wsm_view_move_relative(struct wsm_view *view, int x, int y);
void wsm_view_move(struct wsm_view *view, int x, int y);
void wsm_view_move_to_cursor(struct wsm_view *view);
void wsm_view_moved(struct wsm_view *view);
void wsm_view_move_to_front(struct wsm_view *view);
void wsm_view_move_to_back(struct wsm_view *view);
void wsm_view_move_to_output(struct wsm_view *view, struct wsm_output *output);
void wsm_view_move_to_workspace(struct wsm_view *view, struct wsm_workspace *workspace);
void wsm_view_minimize(struct wsm_view *view, bool minimized);
void wsm_view_maximize(struct wsm_view *view, enum view_axis axis,
                   bool store_natural_geometry);
void wsm_view_adjust_size(struct wsm_view *view, int *w, int *h);
void wsm_view_set_fullscreen(struct wsm_view *view, bool fullscreen);
void wsm_view_set_decorations(struct wsm_view *view, bool decorations);

void wsm_wsm_view_toggle_fullscreen(struct wsm_view *view);
void wsm_view_toggle_maximize(struct wsm_view *view, enum view_axis axis);
void wsm_view_toggle_decorations(struct wsm_view *view);
void wsm_view_toggle_always_on_top(struct wsm_view *view);
void wsm_view_toggle_always_on_bottom(struct wsm_view *view);
void wsm_view_toggle_visible_on_all_workspaces(struct wsm_view *view);

struct wsm_view_size_hints wsm_view_get_size_hints(struct wsm_view *view);
bool wsm_view_is_tiled(struct wsm_view *view);
bool wsm_view_is_floating(struct wsm_view *view);
bool wsm_view_is_always_on_top(struct wsm_view *view);
bool wsm_view_is_always_on_bottom(struct wsm_view *view);

bool wsm_view_on_output(struct wsm_view *view, struct wsm_output *output);

void wsm_view_map(struct wsm_view *view, struct wlr_surface *wlr_surface,
              bool fullscreen, struct wlr_output *fullscreen_output,
              bool decoration);

const char *wsm_view_get_string_prop(struct wsm_view *view, enum wsm_view_prop prop);

enum view_edge wsm_view_edge_invert(enum view_edge edge);
bool wsm_view_is_focusable(struct wsm_view *view);
bool wsm_view_compute_centered_position(struct wsm_view *view,
                                    const struct wlr_box *ref, int w, int h, int *x, int *y);
void wsm_view_store_natural_geometry(struct wsm_view *view);

int wsm_view_effective_height(struct wsm_view *view, bool use_pending);
void wsm_view_center(struct wsm_view *view, const struct wlr_box *ref);
void wsm_view_place_initial(struct wsm_view *view, bool allow_cursor);
void wsm_view_constrain_size_to_that_of_usable_area(struct wsm_view *view);

void wsm_view_restore_to(struct wsm_view *view, struct wlr_box geometry);
void wsm_view_set_untiled(struct wsm_view *view);
bool wsm_view_is_omnipresent(struct wsm_view *view);

void wsm_view_invalidate_last_layout_geometry(struct wsm_view *view);
void wsm_view_adjust_for_layout_change(struct wsm_view *view);
void wsm_view_move_to_edge(struct wsm_view *view, enum view_edge direction, bool snap_to_windows);
void wsm_view_grow_to_edge(struct wsm_view *view, enum view_edge direction);
void wsm_view_shrink_to_edge(struct wsm_view *view, enum view_edge direction);
void wsm_view_snap_to_edge(struct wsm_view *view, enum view_edge direction,
                       bool across_outputs, bool store_natural_geometry);
void wsm_view_snap_to_region(struct wsm_view *view, struct region *region, bool store_natural_geometry);

struct wsm_view *wsm_view_get_root(struct wsm_view *view);
void wsm_view_append_children(struct wsm_view *view, struct wl_array *children);

bool wsm_view_has_strut_partial(struct wsm_view *view);

void wsm_view_reload_ssd(struct wsm_view *view);
void wsm_view_set_shade(struct wsm_view *view, bool shaded);

void wsm_view_evacuate_region(struct wsm_view *view);
void wsm_view_on_output_destroy(struct wsm_view *view);

struct wsm_output *wsm_view_get_adjacent_output(struct wsm_view *view, enum view_edge edge,
                                        bool wrap);
enum view_axis wsm_view_axis_parse(const char *direction);
enum view_edge wsm_view_edge_parse(const char *direction);

#endif
