#include "wsm_server.h"
#include "wsm_log.h"
#include "wsm_xdg_decoration.h"
#include "wsm_xdg_decoration_manager.h"

#include <stdlib.h>

#include <wlr/types/wlr_xdg_decoration_v1.h>

struct wsm_xdg_decoration_manager *xdg_decoration_manager_create(const struct wsm_server* server) {
	struct wsm_xdg_decoration_manager *decoration_manager =
		calloc(1, sizeof(struct wsm_xdg_decoration_manager));
	if (!decoration_manager) {
		wsm_log(WSM_ERROR, "Could not create wsm_xdg_decoration_manager: allocation failed!");
		return NULL;
	}

	wl_list_init(&decoration_manager->xdg_decorations);
	decoration_manager->wlr_xdg_decoration_manager =
		wlr_xdg_decoration_manager_v1_create(server->wl_display);
	decoration_manager->xdg_decoration.notify = handle_xdg_decoration;
	wl_signal_add(&decoration_manager->wlr_xdg_decoration_manager->events.new_toplevel_decoration,
		&decoration_manager->xdg_decoration);

	return decoration_manager;
}
