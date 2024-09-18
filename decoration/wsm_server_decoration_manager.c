#include "wsm_server.h"
#include "wsm_log.h"
#include "wsm_server_decoration.h"
#include "wsm_server_decoration_manager.h"

#include <stdlib.h>

#include <wlr/types/wlr_server_decoration.h>

struct wsm_server_decoration_manager *wsm_server_decoration_manager_create(const struct wsm_server* server) {
	struct wsm_server_decoration_manager *decoration_manager =
		calloc(1, sizeof(struct wsm_server_decoration_manager));
	if (!wsm_assert(decoration_manager, "Could not create wsm_server_decoration_manager: allocation failed!")) {
		return NULL;
	}

	wl_list_init(&decoration_manager->decorations);
	decoration_manager->server_decoration_manager = wlr_server_decoration_manager_create(server->wl_display);
	wlr_server_decoration_manager_set_default_mode(decoration_manager->server_decoration_manager,
		WLR_SERVER_DECORATION_MANAGER_MODE_CLIENT);
	decoration_manager->server_decoration.notify = handle_server_decoration;
	wl_signal_add(&decoration_manager->server_decoration_manager->events.new_decoration,
		&decoration_manager->server_decoration);

	return decoration_manager;
}
