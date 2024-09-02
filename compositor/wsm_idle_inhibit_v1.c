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

#include "wsm_idle_inhibit_v1.h"
#include "wsm_log.h"
#include "wsm_seat.h"
#include "wsm_view.h"
#include "wsm_server.h"
#include "wsm_container.h"
#include "wsm_input_manager.h"

#include <stdlib.h>

#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>

static void destroy_inhibitor(struct wsm_idle_inhibitor_v1 *inhibitor) {
	wl_list_remove(&inhibitor->link);
	wl_list_remove(&inhibitor->destroy.link);
	wsm_idle_inhibit_v1_check_active();
	free(inhibitor);
}

static void handle_destroy(struct wl_listener *listener, void *data) {
	struct wsm_idle_inhibitor_v1 *inhibitor =
		wl_container_of(listener, inhibitor, destroy);
	wsm_log(WSM_DEBUG, "WSM idle inhibitor destroyed");
	destroy_inhibitor(inhibitor);
}

void handle_idle_inhibitor_v1(struct wl_listener *listener, void *data) {
	struct wlr_idle_inhibitor_v1 *wlr_inhibitor = data;
	struct wsm_idle_inhibit_manager_v1 *manager =
		wl_container_of(listener, manager, new_idle_inhibitor_v1);
	wsm_log(WSM_DEBUG, "New wsm idle inhibitor");

	struct wsm_idle_inhibitor_v1 *inhibitor =
		calloc(1, sizeof(struct wsm_idle_inhibitor_v1));
	if (!inhibitor) {
		return;
	}

	inhibitor->mode = INHIBIT_IDLE_APPLICATION;
	inhibitor->wlr_inhibitor = wlr_inhibitor;
	wl_list_insert(&manager->inhibitors, &inhibitor->link);

	inhibitor->destroy.notify = handle_destroy;
	wl_signal_add(&wlr_inhibitor->events.destroy, &inhibitor->destroy);

	wsm_idle_inhibit_v1_check_active();
}

void wsm_idle_inhibit_v1_user_inhibitor_register(struct wsm_view *view,
		enum wsm_idle_inhibit_mode mode) {
	struct wsm_idle_inhibit_manager_v1 *manager =
		&global_server.wsm_idle_inhibit_manager_v1;
	struct wsm_idle_inhibitor_v1 *inhibitor =
			calloc(1, sizeof(struct wsm_idle_inhibitor_v1));
	if (!inhibitor) {
		return;
	}

	inhibitor->mode = mode;
	inhibitor->view = view;
	wl_list_insert(&manager->inhibitors, &inhibitor->link);

	inhibitor->destroy.notify = handle_destroy;
	wl_signal_add(&view->events.unmap, &inhibitor->destroy);

	wsm_idle_inhibit_v1_check_active();
}

struct wsm_idle_inhibitor_v1 *wsm_idle_inhibit_v1_user_inhibitor_for_view(
		struct wsm_view *view) {
	struct wsm_idle_inhibit_manager_v1 *manager = &global_server.wsm_idle_inhibit_manager_v1;
	struct wsm_idle_inhibitor_v1 *inhibitor;
	wl_list_for_each(inhibitor, &manager->inhibitors, link) {
		if (inhibitor->mode != INHIBIT_IDLE_APPLICATION &&
			inhibitor->view == view) {
			return inhibitor;
		}
	}
	return NULL;
}

struct wsm_idle_inhibitor_v1 *wsm_idle_inhibit_v1_application_inhibitor_for_view(
		struct wsm_view *view) {
	struct wsm_idle_inhibit_manager_v1 *manager = &global_server.wsm_idle_inhibit_manager_v1;
	struct wsm_idle_inhibitor_v1 *inhibitor;
	wl_list_for_each(inhibitor, &manager->inhibitors, link) {
		if (inhibitor->mode == INHIBIT_IDLE_APPLICATION &&
			view_from_wlr_surface(inhibitor->wlr_inhibitor->surface) == view) {
			return inhibitor;
		}
	}
	return NULL;
}

void wsm_idle_inhibit_v1_user_inhibitor_destroy(
		struct wsm_idle_inhibitor_v1 *inhibitor) {
	if (!inhibitor) {
		return;
	}
	if (!wsm_assert(inhibitor->mode != INHIBIT_IDLE_APPLICATION,
					"User should not be able to destroy application inhibitor")) {
		return;
	}
	destroy_inhibitor(inhibitor);
}

bool wsm_idle_inhibit_v1_is_active(struct wsm_idle_inhibitor_v1 *inhibitor) {
	switch (inhibitor->mode) {
	case INHIBIT_IDLE_APPLICATION:;
		struct wsm_view *view = view_from_wlr_surface(inhibitor->wlr_inhibitor->surface);
		return !view || !view->container || view_is_visible(view);
	case INHIBIT_IDLE_FOCUS:;
		struct wsm_seat *seat = NULL;
		wl_list_for_each(seat, &global_server.wsm_input_manager->seats, link) {
			struct wsm_container *con = seat_get_focused_container(seat);
			if (con && con->view && con->view == inhibitor->view) {
				return true;
			}
		}
		return false;
	case INHIBIT_IDLE_FULLSCREEN:
		return inhibitor->view->container &&
			   container_is_fullscreen_or_child(inhibitor->view->container) &&
			   view_is_visible(inhibitor->view);
	case INHIBIT_IDLE_OPEN:
		return true;
	case INHIBIT_IDLE_VISIBLE:
		return view_is_visible(inhibitor->view);
	}
	return false;
}

void wsm_idle_inhibit_v1_check_active(void) {
	struct wsm_idle_inhibit_manager_v1 *manager = &global_server.wsm_idle_inhibit_manager_v1;
	struct wsm_idle_inhibitor_v1 *inhibitor;
	bool inhibited = false;
	wl_list_for_each(inhibitor, &manager->inhibitors, link) {
		if ((inhibited = wsm_idle_inhibit_v1_is_active(inhibitor))) {
			break;
		}
	}
	wlr_idle_notifier_v1_set_inhibited(global_server.idle_notifier_v1, inhibited);
}

bool wsm_idle_inhibit_manager_v1_init(void) {
	struct wsm_idle_inhibit_manager_v1 *manager =
		&global_server.wsm_idle_inhibit_manager_v1;

	manager->wlr_manager = wlr_idle_inhibit_v1_create(global_server.wl_display);
	if (!manager->wlr_manager) {
		return false;
	}

	wl_signal_add(&manager->wlr_manager->events.new_inhibitor,
		&manager->new_idle_inhibitor_v1);
	manager->new_idle_inhibitor_v1.notify = handle_idle_inhibitor_v1;
	wl_list_init(&manager->inhibitors);

	return true;
}
