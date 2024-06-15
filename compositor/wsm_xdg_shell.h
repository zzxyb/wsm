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

#ifndef WSM_XDG_SHELL_H
#define WSM_XDG_SHELL_H

#include <wayland-server-core.h>

struct wlr_xdg_shell;
struct wlr_xdg_surface;
struct wlr_xdg_toplevel;
struct wlr_xdg_activation_v1;

struct wsm_view;
struct wsm_server;
struct wsm_xdg_shell_view;

struct wsm_xdg_shell {
    struct wlr_xdg_shell *wlr_xdg_shell;
    struct wlr_xdg_activation_v1 *xdg_activation;
    struct wl_listener xdg_shell_surface;
    struct wl_listener xdg_activation_request;
};

void wsm_xdg_shell_destroy(struct wsm_xdg_shell *shell);
struct wsm_xdg_shell *wsm_xdg_shell_create(const struct wsm_server* server);
struct wsm_xdg_shell_view *xdg_shell_view_from_view(
    struct wsm_view *view);
struct wlr_xdg_toplevel *top_parent_of(struct wsm_view *view);
struct wlr_xdg_surface *xdg_surface_from_view(struct wsm_view *view);
struct wlr_xdg_toplevel *xdg_toplevel_from_view(struct wsm_view *view);

#endif
