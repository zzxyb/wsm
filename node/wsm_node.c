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

#define _POSIX_C_SOURCE 200809L
#include "wsm_node.h"
#include "wsm_log.h"
#include "wsm_view.h"

#include <stdlib.h>
#include <assert.h>

#include <wlr/types/wlr_scene.h>

static void
descriptor_destroy(struct wsm_node_descriptor *node_descriptor)
{
    if (!node_descriptor) {
        return;
    }
    wl_list_remove(&node_descriptor->destroy.link);
    free(node_descriptor);
}

static void
destroy_notify(struct wl_listener *listener, void *data)
{
    struct wsm_node_descriptor *node_descriptor =
        wl_container_of(listener, node_descriptor, destroy);
    descriptor_destroy(node_descriptor);
}

struct wsm_node_descriptor *wsm_node_descriptor_create(struct wlr_scene_node *scene_node,
                                                       enum wsm_node_type type, void *data) {
    struct wsm_node_descriptor *node_descriptor = calloc(1, sizeof(struct wsm_node_descriptor));
    if (!wsm_assert(node_descriptor, "Could not create wsm_node_descriptor: allocation failed!")) {
        return NULL;
    }
    node_descriptor->type = type;
    node_descriptor->data = data;
    node_descriptor->destroy.notify = destroy_notify;
    wl_signal_add(&scene_node->events.destroy, &node_descriptor->destroy);
    scene_node->data = node_descriptor;

    return node_descriptor;
}

struct wsm_view *node_view_from_node(struct wlr_scene_node *wlr_scene_node) {
    assert(wlr_scene_node->data);
    struct wsm_node_descriptor *node_descriptor = wlr_scene_node->data;
    assert(node_descriptor->type == WSM_NODE_VIEW
           || node_descriptor->type == WSM_NODE_XDG_POPUP);
    return (struct wsm_view *)node_descriptor->data;
}
