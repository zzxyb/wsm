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

#ifndef WSM_NODE_H
#define WSM_NODE_H

#include <wayland-server-core.h>

struct wlr_scene_node;

struct wsm_view;

enum wsm_node_type {
    WSM_NODE_VIEW,
    WSM_NODE_XDG_POPUP,
    WSM_NODE_LAYER_SURFACE,
    WSM_NODE_LAYER_POPUP,
    WSM_NODE_IME_POPUP,
    WSM_NODE_TREE,
    WSM_NODE_TEXT,
    WSM_NODE_IMAGE,
};

struct wsm_node_descriptor {
    enum wsm_node_type type;
    void *data;
    struct wl_listener destroy;
};

struct wsm_node_descriptor *wsm_node_descriptor_create(struct wlr_scene_node *scene_node,
                            enum wsm_node_type type, void *data);
struct wsm_view *node_view_from_node(struct wlr_scene_node *wlr_scene_node);

#endif
