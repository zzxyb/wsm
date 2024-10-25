#include "wsm_server.h"
#include "wsm_server_decoration_manager.h"
#include "wsm_server_decoration.h"
#include "wsm_log.h"
#include "wsm_view.h"
#include "wsm_arrange.h"
#include "wsm_container.h"
#include "wsm_transaction.h"

#include <stdlib.h>

#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_compositor.h>

static void server_decoration_handle_destroy(struct wl_listener *listener,
		void *data) {
	struct wsm_server_decoration *deco =
		wl_container_of(listener, deco, destroy);
	wl_list_remove(&deco->destroy.link);
	wl_list_remove(&deco->mode.link);
	wl_list_remove(&deco->link);
	free(deco);
}

static void server_decoration_handle_mode(struct wl_listener *listener,
		void *data) {
	struct wsm_server_decoration *deco =
		wl_container_of(listener, deco, mode);
	struct wsm_view *view =
		view_from_wlr_surface(deco->server_decoration_wlr->surface);
	if (view == NULL || view->surface != deco->server_decoration_wlr->surface) {
		return;
	}

	bool csd = deco->server_decoration_wlr->mode ==
		WLR_SERVER_DECORATION_MANAGER_MODE_CLIENT;
	view_update_csd_from_client(view, csd);

	wsm_arrange_container_auto(view->container);
	transaction_commit_dirty();
}

struct wsm_server_decoration *decoration_from_surface(
	struct wlr_surface *surface) {
	struct wsm_server_decoration *deco;
	wl_list_for_each(deco, &global_server.server_decoration_manager->decorations, link) {
		if (deco->server_decoration_wlr->surface == surface) {
			return deco;
		}
	}
	return NULL;
}

void handle_server_decoration(struct wl_listener *listener, void *data) {
	struct wlr_server_decoration *wlr_deco = data;

	struct wsm_server_decoration *deco = calloc(1, sizeof(struct wsm_server_decoration));
	if (!deco) {
		wsm_log(WSM_ERROR, "Could not create wsm_server_decoration: allocation failed!");
		return;
	}

	deco->server_decoration_wlr = wlr_deco;

	wl_signal_add(&wlr_deco->events.destroy, &deco->destroy);
	deco->destroy.notify = server_decoration_handle_destroy;

	wl_signal_add(&wlr_deco->events.mode, &deco->mode);
	deco->mode.notify = server_decoration_handle_mode;

	wl_list_insert(&global_server.server_decoration_manager->decorations, &deco->link);
}
