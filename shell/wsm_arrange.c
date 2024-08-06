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

#include "wsm_log.h"
#include "wsm_view.h"
#include "wsm_scene.h"
#include "wsm_server.h"
#include "wsm_output.h"
#include "wsm_config.h"
#include "wsm_arrange.h"
#include "wsm_workspace.h"
#include "wsm_common.h"
#include "wsm_titlebar.h"
#include "wsm_layer_shell.h"
#include "wsm_workspace_manager.h"
#include "node/wsm_node_descriptor.h"
#include "node/wsm_text_node.h"

#include <wlr/types/wlr_scene.h>

void arrange_root_auto(void) {
    struct wlr_box layout_box;
    wlr_output_layout_get_box(global_server.wsm_scene->output_layout, NULL, &layout_box);
    global_server.wsm_scene->x = layout_box.x;
    global_server.wsm_scene->y = layout_box.y;
    global_server.wsm_scene->width = layout_box.width;
    global_server.wsm_scene->height = layout_box.height;

    if (global_server.wsm_scene->fullscreen_global) {
        struct wsm_container *fs = global_server.wsm_scene->fullscreen_global;
        fs->pending.x = global_server.wsm_scene->x;
        fs->pending.y = global_server.wsm_scene->y;
        fs->pending.width = global_server.wsm_scene->width;
        fs->pending.height = global_server.wsm_scene->height;
        wsm_arrange_container_auto(fs);
    } else {
        for (int i = 0; i < global_server.wsm_scene->outputs->length; ++i) {
            struct wsm_output *output = global_server.wsm_scene->outputs->items[i];
            wsm_arrange_output_auto(output);
        }
    }
}

void arrange_root_scene(struct wsm_scene *root) {
    struct wsm_container *fs = root->fullscreen_global;

    wlr_scene_node_set_enabled(&root->layers.shell_background->node, !fs);
    wlr_scene_node_set_enabled(&root->layers.shell_bottom->node, !fs);
    wlr_scene_node_set_enabled(&root->layers.tiling->node, !fs);
    wlr_scene_node_set_enabled(&root->layers.floating->node, !fs);
    wlr_scene_node_set_enabled(&root->layers.shell_top->node, !fs);
    wlr_scene_node_set_enabled(&root->layers.fullscreen->node, !fs);

    // hide all contents in the scratchpad
    for (int i = 0; i < root->scratchpad->length; i++) {
        struct wsm_container *con = root->scratchpad->items[i];

        wlr_scene_node_set_enabled(&con->scene_tree->node, false);
    }

    if (fs) {
        for (int i = 0; i < root->outputs->length; i++) {
            struct wsm_output *output = root->outputs->items[i];
            struct wsm_workspace *ws = output->current.active_workspace;

            if (ws) {
                arrange_workspace_floating(ws);
            }
        }

        wsm_arrange_fullscreen(root->layers.fullscreen_global, fs, NULL,
                               root->width, root->height);
    } else {
        for (int i = 0; i < root->outputs->length; i++) {
            struct wsm_output *output = root->outputs->items[i];

            wlr_scene_output_set_position(output->scene_output, output->lx, output->ly);

            wlr_scene_node_reparent(&output->layers.shell_background->node, root->layers.shell_background);
            wlr_scene_node_reparent(&output->layers.shell_bottom->node, root->layers.shell_bottom);
            wlr_scene_node_reparent(&output->layers.tiling->node, root->layers.tiling);
            wlr_scene_node_reparent(&output->layers.shell_top->node, root->layers.shell_top);
            wlr_scene_node_reparent(&output->layers.shell_overlay->node, root->layers.shell_overlay);
            wlr_scene_node_reparent(&output->layers.fullscreen->node, root->layers.fullscreen);
            wlr_scene_node_reparent(&output->layers.session_lock->node, root->layers.session_lock);

            wlr_scene_node_set_position(&output->layers.shell_background->node, output->lx, output->ly);
            wlr_scene_node_set_position(&output->layers.shell_bottom->node, output->lx, output->ly);
            wlr_scene_node_set_position(&output->layers.tiling->node, output->lx, output->ly);
            wlr_scene_node_set_position(&output->layers.fullscreen->node, output->lx, output->ly);
            wlr_scene_node_set_position(&output->layers.shell_top->node, output->lx, output->ly);
            wlr_scene_node_set_position(&output->layers.shell_overlay->node, output->lx, output->ly);
            wlr_scene_node_set_position(&output->layers.session_lock->node, output->lx, output->ly);

            arrange_output_width_size(output, output->width, output->height);
        }
    }

    wsm_arrange_popups(root->layers.popup);
}

void wsm_arrange_output_auto(struct wsm_output *output) {
    struct wlr_box output_box;
    wlr_output_layout_get_box(global_server.wsm_scene->output_layout,
                              output->wlr_output, &output_box);
    output->lx = output_box.x;
    output->ly = output_box.y;
    output->width = output_box.width;
    output->height = output_box.height;

    for (int i = 0; i < output->workspaces->length; ++i) {
        struct wsm_workspace *workspace = output->workspaces->items[i];
        wsm_arrange_workspace_auto(workspace);
    }
}

void arrange_output_width_size(struct wsm_output *output, int width, int height) {
    for (int i = 0; i < output->current.workspaces->length; i++) {
        struct wsm_workspace *child = output->current.workspaces->items[i];

        bool activated = output->current.active_workspace == child;

        wlr_scene_node_reparent(&child->layers.non_fullscreen->node, output->layers.tiling);
        wlr_scene_node_reparent(&child->layers.fullscreen->node, output->layers.fullscreen);

        for (int i = 0; i < child->current.floating->length; i++) {
            struct wsm_container *floater = child->current.floating->items[i];
            wlr_scene_node_reparent(&floater->scene_tree->node, global_server.wsm_scene->layers.floating);
            wlr_scene_node_set_enabled(&floater->scene_tree->node, activated);
        }

        if (activated) {
            struct wsm_container *fs = child->current.fullscreen;
            wlr_scene_node_set_enabled(&child->layers.non_fullscreen->node, !fs);
            wlr_scene_node_set_enabled(&child->layers.fullscreen->node, fs);

            arrange_workspace_floating(child);

            wlr_scene_node_set_enabled(&output->layers.shell_background->node, !fs);
            wlr_scene_node_set_enabled(&output->layers.shell_bottom->node, !fs);
            wlr_scene_node_set_enabled(&output->layers.fullscreen->node, fs);

            if (fs) {
                wlr_scene_rect_set_size(output->fullscreen_background, width, height);

                wsm_arrange_fullscreen(child->layers.fullscreen, fs, child,
                                       width, height);
            } else {
                struct wlr_box *area = &output->usable_area;
                struct side_gaps *gaps = &child->current_gaps;

                wlr_scene_node_set_position(&child->layers.non_fullscreen->node,
                                            gaps->left + area->x, gaps->top + area->y);

                arrange_workspace_tiling(child,
                                         area->width - gaps->left - gaps->right,
                                         area->height - gaps->top - gaps->bottom);
            }
        } else {
            wlr_scene_node_set_enabled(&child->layers.non_fullscreen->node, false);
            wlr_scene_node_set_enabled(&child->layers.fullscreen->node, false);

            disable_workspace(child);
        }
    }
}

void wsm_arrange_workspace_auto(struct wsm_workspace *workspace) {
    if (!workspace->output) {
        // Happens when there are no outputs connected
        return;
    }
    struct wsm_output *output = workspace->output;
    struct wlr_box *area = &output->usable_area;
    wsm_log(WSM_DEBUG, "Usable area for ws: %dx%d@%d,%d",
            area->width, area->height, area->x, area->y);

    bool first_arrange = workspace->width == 0 && workspace->height == 0;
    struct wlr_box prev_box;
    workspace_get_box(workspace, &prev_box);

    double prev_x = workspace->x - workspace->current_gaps.left;
    double prev_y = workspace->y - workspace->current_gaps.top;
    workspace->width = area->width;
    workspace->height = area->height;
    workspace->x = output->lx + area->x;
    workspace->y = output->ly + area->y;

    // Adjust any floating containers
    double diff_x = workspace->x - prev_x;
    double diff_y = workspace->y - prev_y;
    if (!first_arrange && (diff_x != 0 || diff_y != 0)) {
        for (int i = 0; i < workspace->floating->length; ++i) {
            struct wsm_container *floater = workspace->floating->items[i];
            struct wlr_box workspace_box;
            workspace_get_box(workspace, &workspace_box);
            floating_fix_coordinates(floater, &prev_box, &workspace_box);
            // Set transformation for scratchpad windows.
            if (floater->scratchpad) {
                struct wlr_box output_box;
                output_get_box(output, &output_box);
                floater->transform = output_box;
            }
        }
    }

    workspace_add_gaps(workspace);
    node_set_dirty(&workspace->node);
    wsm_log(WSM_DEBUG, "Arranging workspace '%s' at %f, %f", workspace->name,
            workspace->x, workspace->y);
    if (workspace->fullscreen) {
        struct wsm_container *fs = workspace->fullscreen;
        fs->pending.x = output->lx;
        fs->pending.y = output->ly;
        fs->pending.width = output->width;
        fs->pending.height = output->height;
        wsm_arrange_container_auto(fs);
    } else {
        struct wlr_box box;
        workspace_get_box(workspace, &box);
        wsm_arrange_children(workspace->tiling, workspace->layout, &box);
        wsm_arrange_floating(workspace->floating);
    }
}

void wsm_arrange_layer_surface(struct wsm_output *output, const struct wlr_box *full_area,
                               struct wlr_box *usable_area, struct wlr_scene_tree *tree) {
    struct wlr_scene_node *node;
    wl_list_for_each(node, &tree->children, link) {
        struct wsm_layer_surface *surface = wsm_scene_descriptor_try_get(node,
                                                                         WSM_SCENE_DESC_LAYER_SHELL);
        if (!surface) {
            continue;
        }

        if (!surface->scene->layer_surface->initialized) {
            continue;
        }

        wlr_scene_layer_surface_v1_configure(surface->scene, full_area, usable_area);
    }
}


void wsm_arrange_popups(struct wlr_scene_tree *popups) {
    struct wlr_scene_node *node;
    wl_list_for_each(node, &popups->children, link) {
        struct wsm_popup_desc *popup = wsm_scene_descriptor_try_get(node,
                                                                    WSM_SCENE_DESC_POPUP);

        int lx, ly;
        wlr_scene_node_coords(popup->relative, &lx, &ly);
        wlr_scene_node_set_position(node, lx, ly);
    }
}

void wsm_arrange_layers(struct wsm_output *output) {
    struct wlr_box usable_area = { 0 };
    wlr_output_effective_resolution(output->wlr_output,
                                    &usable_area.width, &usable_area.height);
    const struct wlr_box full_area = usable_area;

    wsm_arrange_layer_surface(output, &full_area, &usable_area, output->layers.shell_background);
    wsm_arrange_layer_surface(output, &full_area, &usable_area, output->layers.shell_bottom);
    wsm_arrange_layer_surface(output, &full_area, &usable_area, output->layers.shell_top);
    wsm_arrange_layer_surface(output, &full_area, &usable_area, output->layers.shell_overlay);

    if (!wlr_box_equal(&usable_area, &output->usable_area)) {
        wsm_log(WSM_DEBUG, "Usable area changed, rearranging output");
        output->usable_area = usable_area;
        wsm_arrange_output_auto(output);
    } else {
        wsm_arrange_popups(global_server.wsm_scene->layers.popup);
    }
}

void wsm_arrange_container_auto(struct wsm_container *container) {
    if (container->view) {
        view_autoconfigure(container->view);
        node_set_dirty(&container->node);
        return;
    }
    struct wlr_box box;
    container_get_box(container, &box);
    wsm_arrange_children(container->pending.children, container->pending.layout, &box);
    node_set_dirty(&container->node);
}

static void apply_stacked_layout(struct wsm_list *children, struct wlr_box *parent) {
    if (!children->length) {
        return;
    }
    for (int i = 0; i < children->length; ++i) {
        struct wsm_container *child = children->items[i];
        int parent_offset = child->view ?  0 :
                                container_titlebar_height() * children->length;
        child->pending.x = parent->x;
        child->pending.y = parent->y + parent_offset;
        child->pending.width = parent->width;
        child->pending.height = parent->height - parent_offset;
    }
}

void wsm_arrange_children(struct wsm_list *children,
                          enum wsm_container_layout layout, struct wlr_box *parent) {
    apply_stacked_layout(children, parent);

    for (int i = 0; i < children->length; ++i) {
        struct wsm_container *child = children->items[i];
        wsm_arrange_container_auto(child);
    }
}

void wsm_arrange_floating(struct wsm_list *floating) {
    for (int i = 0; i < floating->length; ++i) {
        struct wsm_container *floater = floating->items[i];
        wsm_arrange_container_auto(floater);
    }
}

void container_arrange_title_bar_node(struct wsm_container *con) {
    enum alignment title_align = ALIGN_CENTER;
    int marks_buffer_width = 0;
    int width = con->title_width;
    int height = container_titlebar_height();

    pixman_region32_t text_area;
    pixman_region32_init(&text_area);

    if (con->title_bar->title_text) {
        struct wsm_text_node *node = con->title_bar->title_text;

        int h_padding;
        if (title_align == ALIGN_RIGHT) {
            h_padding = width - global_config.titlebar_h_padding - node->width;
        } else if (title_align == ALIGN_CENTER) {
            h_padding = ((int)width - marks_buffer_width - node->width) >> 1;
        } else {
            h_padding = global_config.titlebar_h_padding;
        }

        h_padding = MAX(h_padding, 0);

        int alloc_width = MIN((int) node->width,
                              width - h_padding - global_config.titlebar_h_padding);
        alloc_width = MAX(alloc_width, 0);

        wsm_text_node_set_max_width(node, alloc_width);
        wlr_scene_node_set_position(node->node,
                                    h_padding, ((height - node->height) >> 1) + get_max_thickness(con->pending)
                                                                         * con->pending.border_top);
        pixman_region32_union_rect(&text_area, &text_area,
                                   node->node->x, node->node->y, alloc_width, node->height);
    }

    // silence pixman errors
    if (width <= 0 || height <= 0) {
        pixman_region32_fini(&text_area);
        return;
    }

    wlr_scene_node_set_position(&con->title_bar->background->node, 0, get_max_thickness(con->pending)
                                                                         * con->pending.border_top);
    wlr_scene_rect_set_size(con->title_bar->background, width, height);

    container_update(con);
}

void wsm_arrange_title_bar(struct wsm_container *con,
                              int x, int y, int width, int height) {
    container_update(con);

    bool has_title_bar = height > 0;
    wlr_scene_node_set_enabled(&con->title_bar->tree->node, has_title_bar && con->view->enabled);
    if (!has_title_bar) {
        return;
    }

    wlr_scene_node_set_position(&con->title_bar->tree->node, x, y);

    con->title_width = width;
    container_arrange_title_bar_node(con);
}

void wsm_arrange_fullscreen(struct wlr_scene_tree *tree,
                            struct wsm_container *fs, struct wsm_workspace *ws,
                            int width, int height) {
    struct wlr_scene_node *fs_node;
    if (fs->view) {
        fs_node = &fs->view->scene_tree->node;

        // if we only care about the view, disable any decorations
        wlr_scene_node_set_enabled(&fs->scene_tree->node, false);
    } else {
        fs_node = &fs->scene_tree->node;
        wsm_arrange_container_with_title_bar(fs, width, height, true, 0);
    }

    wlr_scene_node_reparent(fs_node, tree);
    wlr_scene_node_lower_to_bottom(fs_node);
    wlr_scene_node_set_position(fs_node, 0, 0);
}

void wsm_arrange_container_with_title_bar(struct wsm_container *con,
                               int width, int height, bool title_bar, int gaps) {
    wlr_scene_node_set_enabled(&con->scene_tree->node, true);

    if (con->output_handler) {
        wlr_scene_buffer_set_dest_size(con->output_handler, width, height);
    }

    if (con->view) {
        int max_thickness = get_max_thickness(con->current);
        int border_top = container_titlebar_height() + max_thickness * con->current.border_top;
        int border_width = max_thickness;

        if (con->current.border == B_NORMAL) {
            if (title_bar) {
                wsm_arrange_title_bar(con, max_thickness, 0, width - max_thickness * 2, border_top);
            } else {
                border_top = 0;
                // should be handled by the parent container
            }
        } else if (con->current.border == B_NONE) {
            container_update(con);
            border_top = 0;
            border_width = 0;
        } else if (con->current.border == B_CSD) {
            border_top = 0;
            border_width = 0;
        } else {
            wsm_assert(false, "unreachable");
        }

        int border_bottom = con->current.border_bottom ? border_width : 0;
        int border_left = con->current.border_left ? border_width : 0;
        int border_right = con->current.border_right ? border_width : 0;
        int page_top = con->current.border_top ? border_width : 0;

        wlr_scene_rect_set_size(con->sensing.top, width, page_top);
        wlr_scene_rect_set_size(con->sensing.bottom, width, border_bottom);
        wlr_scene_rect_set_size(con->sensing.left,
                                border_left, height - border_bottom - page_top);
        wlr_scene_rect_set_size(con->sensing.right,
                                border_right, height - border_bottom - page_top);

        wlr_scene_node_set_position(&con->sensing.top->node, 0, 0);
        wlr_scene_node_set_position(&con->sensing.bottom->node,
                                    0, height - border_bottom);
        wlr_scene_node_set_position(&con->sensing.left->node,
                                    0, page_top);
        wlr_scene_node_set_position(&con->sensing.right->node,
                                    width - border_right, page_top);

        // make sure to reparent, it's possible that the client just came out of
        // fullscreen mode where the parent of the surface is not the container
        wlr_scene_node_reparent(&con->view->scene_tree->node, con->content_tree);
        wlr_scene_node_set_position(&con->view->scene_tree->node,
                                    border_left, border_top);
    } else {
        // make sure to disable the title bar if the parent is not managing it
        if (title_bar) {
            wlr_scene_node_set_enabled(&con->title_bar->tree->node, false);
        }

        arrange_children_with_titlebar(con->current.layout, con->current.children,
                          con->current.focused_inactive_child, con->content_tree,
                          width, height, gaps);
    }
}

void arrange_children_with_titlebar(enum wsm_container_layout layout, struct wsm_list *children,
                              struct wsm_container *active, struct wlr_scene_tree *content,
                              int width, int height, int gaps) {
    int title_bar_height = container_titlebar_height();

    struct wsm_container *first = children->length == 1 ?
                                      ((struct wsm_container *)children->items[0]) : NULL;
    if (first && first->view &&
        first->current.border != B_NORMAL) {
        title_bar_height = 0;
    }

    int title_height = title_bar_height * children->length;

    int y = 0;
    for (int i = 0; i < children->length; i++) {
        struct wsm_container *child = children->items[i];
        bool activated = child == active;

        wsm_arrange_title_bar(child, 0, y + title_height, width, title_bar_height);
        wlr_scene_node_set_enabled(&child->sensing.tree->node, activated);
        wlr_scene_node_set_position(&child->scene_tree->node, 0, title_height);
        wlr_scene_node_reparent(&child->scene_tree->node, content);

        if (activated) {
            wsm_arrange_container_with_title_bar(child, width, height - title_height,
                               false, 0);
        } else {
            disable_container(child);
        }

        y += title_bar_height;
    }
}

void arrange_workspace_tiling(struct wsm_workspace *ws,
                                     int width, int height) {
    arrange_children_with_titlebar(ws->current.layout, ws->current.tiling,
                                   ws->current.focused_inactive_child, ws->layers.non_fullscreen,
                                   width, height, ws->gaps_inner);
}

void arrange_workspace_floating(struct wsm_workspace *ws) {
    for (int i = 0; i < ws->current.floating->length; i++) {
        struct wsm_container *floater = ws->current.floating->items[i];
        struct wlr_scene_tree *layer = global_server.wsm_scene->layers.floating;

        if (floater->current.fullscreen_mode != FULLSCREEN_NONE) {
            continue;
        }

        if (global_server.wsm_scene->fullscreen_global) {
            if (container_is_transient_for(floater, global_server.wsm_scene->fullscreen_global)) {
                layer = global_server.wsm_scene->layers.fullscreen_global;
            }
        } else {
            for (int i = 0; i < global_server.wsm_scene->outputs->length; i++) {
                struct wsm_output *output = global_server.wsm_scene->outputs->items[i];
                struct wsm_workspace *active = output->current.active_workspace;

                if (active && active->fullscreen &&
                    container_is_transient_for(floater, active->fullscreen)) {
                    layer = global_server.wsm_scene->layers.fullscreen;
                }
            }
        }

        wlr_scene_node_reparent(&floater->scene_tree->node, layer);
        wlr_scene_node_set_position(&floater->scene_tree->node,
                                    floater->current.x, floater->current.y);
        wlr_scene_node_set_enabled(&floater->scene_tree->node, true);

        wsm_arrange_container_with_title_bar(floater, floater->current.width, floater->current.height,
                                             true, ws->gaps_inner);
    }
}
