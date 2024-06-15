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

#ifndef WSM_NODE_DESCRIPTOR_H
#define WSM_NODE_DESCRIPTOR_H

#include <wayland-server-core.h>

struct wlr_scene_node;

struct wsm_view;

enum wsm_scene_descriptor_type {
    WSM_SCENE_DESC_VIEW,
    WSM_SCENE_DESC_LAYER_SHELL,
    WSM_SCENE_DESC_XWAYLAND_UNMANAGED,
    WSM_SCENE_DESC_POPUP,
    WSM_SCENE_DESC_DRAG_ICON,
};

bool wsm_scene_descriptor_assign(struct wlr_scene_node *node,
                             enum wsm_scene_descriptor_type type, void *data);

void *wsm_scene_descriptor_try_get(struct wlr_scene_node *node,
                               enum wsm_scene_descriptor_type type);

void wsm_scene_descriptor_destroy(struct wlr_scene_node *node,
                              enum wsm_scene_descriptor_type type);

#endif
