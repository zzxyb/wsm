#ifndef WSM_WORKSPACE_MANAGER_H
#define WSM_WORKSPACE_MANAGER_H

#include <wayland-server-core.h>

struct wsm_workspace;
struct wsm_output;
struct wsm_list;

struct wsm_server;

/**
 * @brief workspace management.
 *
 * @details record the status between current workspaces,
 * manage all wsm_output.
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
};

struct wsm_workspace_manager *wsm_workspace_manager_create(const struct wsm_server* server,
	struct wsm_output *output);
void wsm_workspace_manager_destroy(struct wsm_workspace_manager *workspace_manager);
void workspaces_switch_to(struct wsm_workspace *target, bool update_focus);
void workspaces_switch_pre(struct wsm_workspace *target, bool update_focus);
void workspaces_switch_next(struct wsm_workspace *target, bool update_focus);
bool workspaces_has_pre(struct wsm_workspace *pre_workspace);
bool workspaces_has_next(struct wsm_workspace *next_workspace);

#endif
