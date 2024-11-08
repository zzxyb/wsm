#ifndef WSM_WORKSPACE_MANAGER_H
#define WSM_WORKSPACE_MANAGER_H

#include <wayland-server-core.h>

struct wsm_workspace;
struct wsm_output;
struct wsm_list;

struct wsm_server;

 /* -------------------------------------------------------------------------
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
 * -------------------------------------------------------------------------*/

/**
 * @brief Workspace management structure.
 *
 * @details This structure records the status between current workspaces
 * and manages all wsm_output.
 */
struct wsm_workspace_manager {
	// This structure currently has no members but can be extended in the future.
};

/**
 * @brief Creates a new workspace manager.
 * @param server Pointer to the WSM server instance.
 * @param output Pointer to the associated output.
 * @return Pointer to the newly created wsm_workspace_manager instance.
 */
struct wsm_workspace_manager *wsm_workspace_manager_create(const struct wsm_server* server,
	struct wsm_output *output);

/**
 * @brief Destroys the specified workspace manager.
 * @param workspace_manager Pointer to the wsm_workspace_manager instance to destroy.
 */
void wsm_workspace_manager_destroy(struct wsm_workspace_manager *workspace_manager);

/**
 * @brief Switches to the specified workspace.
 * @param target Pointer to the target wsm_workspace to switch to.
 * @param update_focus Flag indicating if focus should be updated.
 */
void workspaces_switch_to(struct wsm_workspace *target, bool update_focus);

/**
 * @brief Switches to the previous workspace.
 * @param target Pointer to the target wsm_workspace to switch to.
 * @param update_focus Flag indicating if focus should be updated.
 */
void workspaces_switch_pre(struct wsm_workspace *target, bool update_focus);

/**
 * @brief Switches to the next workspace.
 * @param target Pointer to the target wsm_workspace to switch to.
 * @param update_focus Flag indicating if focus should be updated.
 */
void workspaces_switch_next(struct wsm_workspace *target, bool update_focus);

/**
 * @brief Checks if there is a previous workspace.
 * @param pre_workspace Pointer to the previous workspace to check.
 * @return true if there is a previous workspace, false otherwise.
 */
bool workspaces_has_pre(struct wsm_workspace *pre_workspace);

/**
 * @brief Checks if there is a next workspace.
 * @param next_workspace Pointer to the next workspace to check.
 * @return true if there is a next workspace, false otherwise.
 */
bool workspaces_has_next(struct wsm_workspace *next_workspace);

#endif
