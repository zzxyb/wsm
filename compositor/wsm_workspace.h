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

#ifndef WSM_WORKSPACE_H
#define WSM_WORKSPACE_H

#include <stdbool.h>

struct wlr_box;
struct wlr_surface;
struct wlr_scene_tree;

struct wsm_list;
struct wsm_output;
struct wsm_xwayland_view;

struct wsm_workspace_state {
    struct wsm_containers *fullscreen;
    double x, y;
    int width, height;
    bool focused;
};

/**
 * @brief workspace object.
 *
 * @details multiple wsm_containers are arranged in layers
 * in wsm_workspace to achieve a stacking effect.
 * -------------------------------------------------
 * |                                                |
 * |                wsm_workspace                   |
 * |                                                |
 * |    ***********                                 |
 * |    *         *  wsm_container1 mean layer1     |
 * |    *         *                                 |
 * |    *   ***********                             |
 * |    *   *     *   *                             |
 * |    *   *     *   * wsm_container2 mean layer2  |
 * |    ***********   *                             |
 * |        *         *                             |
 * |        *         *                             |
 * |        ***********                             |
 * |                         container3....max      |
 * --------------------------------------------------
 */
struct wsm_workspace {
    int index;

    double x, y;
    int width, height;
    struct wsm_output *output;
    struct wsm_list *wsm_containers;
    struct wlr_scene_tree *tree;
};

#endif
