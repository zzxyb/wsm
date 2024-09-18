#include "wsm_log.h"
#include "wsm_scene.h"
#include "wsm_server.h"
#include "wsm_output.h"
#include "wsm_workspace.h"
#include "wsm_workspace_manager.h"

#include <stdlib.h>

#include <wlr/types/wlr_scene.h>

struct wsm_workspace_manager *wsm_workspace_manager_create(const struct wsm_server* server,
		struct wsm_output *output) {
	struct wsm_workspace_manager *workspace_manager =
		calloc(1, sizeof(struct wsm_workspace_manager));
	if (!wsm_assert(workspace_manager,
			"Could not create wsm_workspace_manager: allocation failed!")) {
		return NULL;
	}

	return workspace_manager;
}

void wsm_workspace_manager_destroy(struct wsm_workspace_manager *workspace_manager) {
	if (!workspace_manager) {
		wsm_log(WSM_ERROR, "workspace_manager is NULL!");
	};

	free(workspace_manager);
}
