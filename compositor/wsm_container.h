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

#include <wayland-server-core.h>

struct wsm_view;
struct wsm_view_item;

/**
 * @brief mark wsm_view_item layout status in wsm_container
 *
 * @details if the current wsm_container has only one wsm_view_item,
 * then wsm_container is equivalent to one wsm_view_item.note that
 * there are still layer levels between wsm_view_item at this time
 */
enum wsm_view_layout_mode {
    L_NONE, /**< no layout, at this time wsm_container is equivalent to wsm_view_item. */
    L_HORIZ, /**< two wsm_view_item horizontal layout. */
    L_HORIZ_1_V_2, /**< three wsm_view_item horizontal layout.left : right = 1 : 2. */
    L_HORIZ_2_V_1, /**< three wsm_view_item horizontal layout.left : right = 2 : 1. */
    L_GRID, /**< four wsm_view_item grid layout. */
};

struct wsm_container_state {
    enum wsm_view_layout_mode layout;
    double x, y;
    double width, height;
    struct wl_list view_items;
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
    struct wsm_container_state pending;
    struct wl_list outputs;
    struct {
        struct wl_signal destroy;
    } events;
};

struct wsm_container *container_create(struct wsm_view *view);
void container_destroy(struct wsm_view *con);

#endif
