#include "wsm_titlebar.h"
#include "wsm_log.h"
#include "node/wsm_text_node.h"

#include <stdlib.h>

#include <wlr/types/wlr_scene.h>

struct wsm_titlebar* wsm_titlebar_create() {
	struct wsm_titlebar *titlebar = calloc(1, sizeof(struct wsm_titlebar));
	if (!wsm_assert(titlebar, "Could not create wsm_titlebar: allocation failed!")) {
		return NULL;
	}

	return titlebar;
}

void wsm_titlebar_destroy(struct wsm_titlebar *titlebar) {
	wlr_scene_node_destroy(&titlebar->tree->node);
	free(titlebar);
}
