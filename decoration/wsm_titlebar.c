#include "wsm_titlebar.h"
#include "wsm_log.h"
#include "node/wsm_text_node.h"

#include <stdlib.h>

#include <wlr/types/wlr_scene.h>

struct wsm_titlebar* wsm_titlebar_create() {
	struct wsm_titlebar *titlebar = calloc(1, sizeof(struct wsm_titlebar));
	if (!titlebar) {
		wsm_log(WSM_ERROR, "Could not create wsm_titlebar: allocation failed!");
		return NULL;
	}

	wl_signal_init(&titlebar->events.double_click);
	wl_signal_init(&titlebar->events.request_state);
	wl_list_init(&titlebar->min_button_clicked.link);
	wl_list_init(&titlebar->max_button_clicked.link);
	wl_list_init(&titlebar->close_button_clicked.link);
	wl_list_init(&titlebar->double_click.link);

	return titlebar;
}

void wsm_titlebar_destroy(struct wsm_titlebar *titlebar) {
	if (!wl_list_empty(&titlebar->min_button_clicked.link)) {
		wl_list_remove(&titlebar->min_button_clicked.link);
	}
	if (!wl_list_empty(&titlebar->max_button_clicked.link)) {
		wl_list_remove(&titlebar->max_button_clicked.link);
	}
	if (!wl_list_empty(&titlebar->close_button_clicked.link)) {
		wl_list_remove(&titlebar->close_button_clicked.link);
	}
	if (!wl_list_empty(&titlebar->double_click.link)) {
		wl_list_remove(&titlebar->double_click.link);
	}

	wlr_scene_node_destroy(&titlebar->tree->node);
	free(titlebar);
}
