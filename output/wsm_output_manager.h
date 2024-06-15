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

#ifndef WSM_OUTPUT_MANAGER_H
#define WSM_OUTPUT_MANAGER_H

/*
------------------------------------------------------------------------
|                                                                       |
|                          wsm_output_manager                           |
|                                                                       |
|    -------------------------------------------------------------      |
|    |                   |                   |                          |
|    |                   |                   |                          |
|    |     output1       |      output2      |       3、4、5....         |
|    |                   |                   |                          |
|    |                   |                   |                          |
|    --------------------------------------------------------------     |
|                                                                       |
------------------------------------------------------------------------- */

#include <wayland-server-core.h>

struct wl_listener;
struct wlr_output_layout;
struct wlr_output_manager_v1;
struct wlr_gamma_control_manager_v1;
struct wlr_output_power_manager_v1;

struct wsm_server;
struct wsm_output_manager_config;

struct wsm_output_manager {
    struct wlr_output_manager_v1 *wlr_output_manager_v1;
    struct wlr_output_layout *wlr_output_layout;
    struct wlr_output_power_manager_v1 *wlr_output_power_manager_v1;
    struct wlr_gamma_control_manager_v1 *wlr_gamma_control_manager_v1;
    struct wsm_output_manager_config *wsm_output_manager_config;
    struct wl_list views;
    struct wl_list unmanaged_surfaces;
    struct wl_list outputs;

    struct wl_listener new_output;
    struct wl_listener output_layout_change;
    struct wl_listener output_manager_apply;
    struct wl_listener output_manager_test;
    struct wl_listener wsm_output_power_manager_set_mode;

    struct wl_listener gamma_control_set_gamma;

    int pending_output_layout_change;
};

struct wsm_output_manager *wsm_output_manager_create(const struct wsm_server *server);
void wsm_output_manager_destory(struct wsm_output_manager *manager);

#endif
