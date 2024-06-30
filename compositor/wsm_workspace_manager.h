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

#ifndef WSM_WORKSPACE_MANAGER_H
#define WSM_WORKSPACE_MANAGER_H

#include <wayland-server-core.h>

struct wsm_workspace;
struct wsm_output;
struct wsm_list;

struct wsm_server;

struct wsm_workspace_manager_state {
    struct wsm_list *workspaces;
    struct wsm_workspace *active_workspace;
};

/**
 * @brief workspace management.
 *
 * @details record the status between current workspaces,
 * add and delete workspaces.a wsm_output can only have
 * one wsm_workspace_manager.generally, the size of
 * wsm_workspace is the same as wsm_output.
 * -------------------------------------------------------------------------
 * |                                                                       |
 * |                     wsm_workspace_manager                             |
 * |                                    *******************                |
 * |    --------------------------------*-----------------*----------      |
 * |    |      (default)    |           *        |        *                |
 * |    |                   |           *        |        *                |
 * |    |  wsm_workspace0   |  wsm_workspace2    |       2、3、4....        |
 * |    |                   |           *        |        *                |
 * |    |                   |           *        |        *                |
 * |    --------------------------------*-----------------*-----------     |
 * |                                    ******************* wsm_output     |
 * -------------------------------------------------------------------------
 */
struct wsm_workspace_manager {
    struct wsm_workspace_manager_state current;
};

struct wsm_workspace_manager *wsm_workspace_manager_create(const struct wsm_server* server,
                                                           struct wsm_output *output);
void wsm_workspace_manager_destroy(struct wsm_workspace_manager *workspace_manager);

#endif
