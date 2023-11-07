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
#include "wsm_log.h"
#include "wsm_scene.h"
#include "wsm_server.h"
#include "wsm_text_node.h"
#include "wsm_image_node.h"
#include "wsm_output_manager.h"

#include <stdlib.h>
#include <assert.h>

#include <wlr/types/wlr_scene.h>

struct wsm_scene *wsm_scene_create() {
    struct wsm_scene *scene = calloc(1, sizeof(struct wsm_scene));
    if (!wsm_assert(scene, "Could not create wsm_scene: allocation failed!")) {
        return NULL;
    }
    scene->wlr_scene = wlr_scene_create();
    if (!wsm_assert(scene->wlr_scene, "wlr_scene_create return NULL!")) {
        return NULL;
    }

    scene->view_tree_always_on_bottom = wlr_scene_tree_create(&scene->wlr_scene->tree);
    assert(scene->view_tree_always_on_bottom);

    scene->view_tree = wlr_scene_tree_create(&scene->wlr_scene->tree);
    assert(scene->view_tree);

    scene->view_tree_always_on_top = wlr_scene_tree_create(&scene->wlr_scene->tree);
    assert(scene->view_tree_always_on_top);

#ifdef HAVE_XWAYLAND
    scene->unmanaged_tree = wlr_scene_tree_create(&scene->wlr_scene->tree);
    assert(scene->unmanaged_tree);
#endif

    scene->xdg_popup_tree = wlr_scene_tree_create(&scene->wlr_scene->tree);
    assert(scene->xdg_popup_tree);

    scene->osd_overlay_layers_tree = wlr_scene_tree_create(&scene->wlr_scene->tree);
    assert(scene->osd_overlay_layers_tree);

    // titlebar test
    // float rec_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    // struct wlr_scene_rect *rect = wlr_scene_rect_create(scene->osd_overlay_layers_tree, 400, 20, rec_color);
    // wlr_scene_node_set_position(&rect->node, 20, 200);

    // text
    // float color[4] = {1.0f, 0.0f, 0.0f, 1.0f}; // 黑色，alpha 值为 1.0
    // struct wsm_text_node *nn = wsm_text_node_create(scene->osd_overlay_layers_tree, "1111111", color, true);
    // wlr_scene_node_set_position(nn->node, 200, 200);

    // image
    // struct wsm_image_node *image = wsm_image_node_create(scene->osd_overlay_layers_tree, 342, 203, "/home/xyb/Desktop/1.png", 1.0);
    // struct wsm_image_node *image = wsm_image_node_create(scene->osd_overlay_layers_tree, 400, 400, "/home/xyb/Desktop/test.JPG", 1.0);
    // struct wsm_image_node *image = wsm_image_node_create(scene->osd_overlay_layers_tree, 972, 713, "/home/xyb/Desktop/111.svg", 1.0);
    // wlr_scene_node_set_position(image->node, 0, 0);
    // wsm_image_node_set_size(image, 271, 101);

    scene->wlr_scene_output_layout = wlr_scene_attach_output_layout(scene->wlr_scene, server.wsm_output_manager->wlr_output_layout);
    return scene;
}
