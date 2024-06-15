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
#include "wsm_output.h"
#include "wsm_workspace.h"
#include "wsm_workspace_manager.h"

#include <stdlib.h>

#include <wlr/types/wlr_scene.h>

struct wsm_workspace_manager *wsm_workspace_manager_create(const struct wsm_server* server, struct wsm_output *output) {
    struct wsm_workspace_manager *workspace_manager = calloc(1, sizeof(struct wsm_workspace_manager));
    if (!wsm_assert(workspace_manager, "Could not create wsm_workspace_manager: allocation failed!")) {
        return NULL;
    }

    wl_list_init(&workspace_manager->wsm_workspaces);

    struct wsm_workspace *workspace = calloc(1, sizeof(struct wsm_workspace));
    workspace_manager->active_workspace = workspace;
    workspace->index = 0;
    workspace->x = 0;
    workspace->y = 0;
    workspace->output = output;
    workspace->height = output->wlr_output->height;
    workspace->width = output->wlr_output->width;
    workspace->tree = wlr_scene_tree_create(server->wsm_scene->view_tree);
    wlr_scene_node_set_enabled(&workspace->tree->node, true);

    return workspace_manager;
}

void wsm_workspace_destroy(struct wsm_workspace_manager *workspace_manager) {
    if (!workspace_manager) {
        wsm_log(WSM_ERROR, "workspace_manager is NULL!");
    };

    wl_list_remove(&workspace_manager->wsm_workspaces);
    free(workspace_manager);
}
