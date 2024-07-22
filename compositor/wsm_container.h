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

#ifndef WSM_CONTAINER_H
#define WSM_CONTAINER_H

#include "node/wsm_node.h"
#include "wsm_titlebar.h"

#include <wayland-server-core.h>

#include <wlr/util/box.h>

#define MIN_SANE_W 100
#define MIN_SANE_H 60

struct wlr_scene_buffer;

struct wsm_view;
struct wsm_view_item;

/**
 * @brief mark wsm_view_item layout status in wsm_container
 *
 * @details if the current wsm_container has only one wsm_view_item,
 * then wsm_container is equivalent to one wsm_view_item.note that
 * there are still layer levels between wsm_view_item at this time
 */
enum wsm_container_layout {
    L_NONE, /**< no layout, at this time wsm_container is equivalent to wsm_view_item. */
    L_HORIZ, /**< two wsm_view_item horizontal layout. */
    L_HORIZ_1_V_2, /**< three wsm_view_item horizontal layout.left : right = 1 : 2. */
    L_HORIZ_2_V_1, /**< three wsm_view_item horizontal layout.left : right = 2 : 1. */
    L_GRID, /**< four wsm_view_item grid layout. */
};

enum wsm_container_border {
    B_NONE,
    B_PIXEL,
    B_NORMAL,
    B_CSD,
};

enum wsm_fullscreen_mode {
    FULLSCREEN_NONE,
    FULLSCREEN_WORKSPACE,
    FULLSCREEN_GLOBAL,
};

enum alignment {
    ALIGN_LEFT,
    ALIGN_CENTER,
    ALIGN_RIGHT,
};

struct border_colors {
    float border[4];
    float background[4];
    float text[4];
    float indicator[4];
    float child_border[4];
};

struct wsm_container_state {
    enum wsm_container_layout layout;
    double x, y;
    double width, height;

    enum wsm_fullscreen_mode fullscreen_mode;

    struct wsm_workspace *workspace;
    struct wsm_container *parent;
    struct wsm_list *children;

    struct wsm_container *focused_inactive_child;
    bool focused;

    enum wsm_container_border border;
    int border_thickness;
    bool border_top;
    bool border_bottom;
    bool border_left;
    bool border_right;

    // These are in layout coordinates.
    double content_x, content_y;
    double content_width, content_height;
};

/**
 * @brief container for wsm_view_items.
 *
 * @details At the same time, implement the function of
 * wsm_view_items layout.
 *
 * L_NONE looks like this:
 * ------------------------
 * |                      |
 * | wsm_container L_NONE |
 * |                      |
 * | ******************   |
 * | *                *   |
 * | *  wsm_view_item *   |
 * | *                *   |
 * | ******************   |
 * ------------------------
 *
 * L_HORIZ looks like this:
 * --------------------------------------------
 * |           wsm_container L_HORIZ          |
 * |                                          |
 * |                   split                  |
 * | ****************** |  ****************** |
 * | *                * |  *                * |
 * | *  view_item1    * |  *   view_item2   * |
 * | *                * |  *                * |
 * | ****************** |  ****************** |
 * --------------------------------------------
 *
 * L_HORIZ_1_V_2 looks like this:
 * --------------------------------------------
 * |       wsm_container L_HORIZ_1_V_2        |
 * |                                          |
 * |                   split                  |
 * | ****************** |  ****************** |
 * | *                * |  *                * |
 * | *                * |  *   view_item2   * |
 * | *                * |  *                * |
 * | *                * |  ****************** |
 * | *                * |                     |
 * | *   view_item1   * |  ---------split-----|
 * | *                * |                     |
 * | *                * |  ****************** |
 * | *                * |  *                * |
 * | *                * |  *   view_item3   * |
 * | *                * |  *                * |
 * | ****************** |  ****************** |
 * --------------------------------------------
 *
 * L_HORIZ_2_V_1 looks like this:
 * --------------------------------------------
 * |       wsm_container L_HORIZ_2_V_1        |
 * |                                          |
 * |                   split                  |
 * | ****************** |  ****************** |
 * | *                * |  *                * |
 * | *  view_item2    * |  *                * |
 * | *                * |  *                * |
 * | ****************** |  *                * |
 * |                    |  *                * |
 * | ------split--------|  *   view_item1   * |
 * |                    |  *                * |
 * | ****************** |  *                * |
 * | *                * |  *                * |
 * | *  view_item3    * |  *                * |
 * | *                * |  *                * |
 * | ****************** |  ****************** |
 * --------------------------------------------
 *
 * L_GRID looks like this:
 * --------------------------------------------
 * |           wsm_container L_GRID           |
 * |                                          |
 * |                   split                  |
 * | ****************** |  ****************** |
 * | *                * |  *                * |
 * | *  view_item1    * |  *   view_item3   * |
 * | *                * |  *                * |
 * | ****************** |  ****************** |
 * |                                          |
 * | ------split--------   --------split----- |
 * |                   split                  |
 * | ****************** |  ****************** |
 * | *                * |  *                * |
 * | *  view_item2    * |  *   view_item4   * |
 * | *                * |  *                * |
 * | ****************** |  ****************** |
 * --------------------------------------------
 */
struct wsm_container {
    struct wsm_node node;
    struct wsm_view *view;

    struct wlr_scene_tree *scene_tree;

    struct wsm_titlebar title_bar;

    struct {
        struct wlr_scene_tree *tree;

        struct wlr_scene_rect *top;
        struct wlr_scene_rect *bottom;
        struct wlr_scene_rect *left;
        struct wlr_scene_rect *right;
    } border;

    struct wlr_scene_tree *content_tree;
    struct wlr_scene_buffer *output_handler;

    struct wl_listener output_enter;
    struct wl_listener output_leave;

    struct wsm_container_state current;
    struct wsm_container_state pending;

    char *title;           // The view's title (unformatted)
    char *formatted_title; // The title displayed in the title bar
    int title_width;

    enum wsm_container_layout prev_split_layout;

    // Whether stickiness has been enabled on this container. Use
    // `container_is_sticky_[or_child]` rather than accessing this field
    // directly; it'll also check that the container is floating.
    bool is_sticky;

    // For C_ROOT, this has no meaning
    // For other types, this is the position in layout coordinates
    // Includes borders
    double saved_x, saved_y;
    double saved_width, saved_height;

    // Used when the view changes to CSD unexpectedly. This will be a non-B_CSD
    // border which we use to restore when the view returns to SSD.
    enum wsm_container_border saved_border;

    // The share of the space of parent container this container occupies
    double width_fraction;
    double height_fraction;

    // The share of space of the parent container that all children occupy
    // Used for doing the resize calculations
    double child_total_width;
    double child_total_height;

    // Indicates that the container is a scratchpad container.
    // Both hidden and visible scratchpad containers have scratchpad=true.
    // Hidden scratchpad containers have a NULL parent.
    bool scratchpad;

    // Stores last output size and position for adjusting coordinates of
    // scratchpad windows.
    // Unused for non-scratchpad windows.
    struct wlr_box transform;

    float alpha;

    struct wsm_list *marks; // char *

    struct {
        struct wl_signal destroy;
    } events;
};

struct wsm_container *container_create(struct wsm_view *view);
void container_destroy(struct wsm_container *con);
void container_begin_destroy(struct wsm_container *con);
void container_get_box(struct wsm_container *container, struct wlr_box *box);
struct wsm_container *container_find_child(struct wsm_container *container,
                                            bool (*test)(struct wsm_container *view, void *data), void *data);

void container_for_each_child(struct wsm_container *container,
                              void (*f)(struct wsm_container *container, void *data), void *data);
bool container_is_fullscreen_or_child(struct wsm_container *container);
bool container_is_scratchpad_hidden(struct wsm_container *con);
bool container_is_scratchpad_hidden_or_child(struct wsm_container *con);
struct wsm_container *container_toplevel_ancestor(
    struct wsm_container *container);
bool container_is_floating_or_child(struct wsm_container *container);
bool container_is_floating(struct wsm_container *container);
size_t container_titlebar_height(void);
void container_raise_floating(struct wsm_container *con);
void container_fullscreen_disable(struct wsm_container *con);
void container_floating_move_to(struct wsm_container *con,
                                double lx, double ly);
void container_floating_move_to_center(struct wsm_container *con);
void container_floating_translate(struct wsm_container *con,
                                  double x_amount, double y_amount);
void container_floating_resize_and_center(struct wsm_container *con);
void container_set_geometry_from_content(struct wsm_container *con);
void container_end_mouse_operation(struct wsm_container *container);
bool container_has_ancestor(struct wsm_container *descendant,
                            struct wsm_container *ancestor);
void container_detach(struct wsm_container *child);
struct wsm_list *container_get_siblings(struct wsm_container *container);
void container_update_representation(struct wsm_container *container);
size_t container_build_representation(enum wsm_container_layout layout,
                                      struct wsm_list *children, char *buffer);
void container_arrange_title_bar(struct wsm_container *con);
void container_update_title_bar(struct wsm_container *container);
void container_handle_fullscreen_reparent(struct wsm_container *con);
void floating_fix_coordinates(struct wsm_container *con,
                              struct wlr_box *old, struct wlr_box *new);
enum wsm_container_layout container_parent_layout(struct wsm_container *con);
void container_set_fullscreen(struct wsm_container *con,
                              enum wsm_fullscreen_mode mode);
void container_reap_empty(struct wsm_container *con);
void root_scratchpad_remove_container(struct wsm_container *con);
void container_add_sibling(struct wsm_container *parent,
                           struct wsm_container *child, bool after);
void container_set_floating(struct wsm_container *container, bool enable);
void container_floating_set_default_size(struct wsm_container *con);
void container_add_child(struct wsm_container *parent,
                         struct wsm_container *child);
bool container_is_sticky(struct wsm_container *con);
bool container_is_sticky_or_child(struct wsm_container *con);
bool container_is_transient_for(struct wsm_container *child,
                                struct wsm_container *ancestor);
void container_update(struct wsm_container *con);
void container_update_itself_and_parents(struct wsm_container *con);
bool container_has_urgent_child(struct wsm_container *container);
void container_set_resizing(struct wsm_container *con, bool resizing);
void floating_calculate_constraints(int *min_width, int *max_width,
                                    int *min_height, int *max_height);
struct wsm_container *container_obstructing_fullscreen_container(struct wsm_container *container);
void disable_container(struct wsm_container *con);

#endif
