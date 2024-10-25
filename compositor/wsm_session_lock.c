#include "wsm_session_lock.h"
#include "wsm_log.h"
#include "wsm_seat.h"
#include "wsm_server.h"
#include "wsm_output.h"
#include "wsm_scene.h"
#include "wsm_cursor.h"
#include "wsm_arrange.h"
#include "wsm_input_manager.h"
#include "wsm_layer_shell.h"

#include <stdlib.h>

#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_session_lock_v1.h>

struct wsm_session_lock_output {
	struct wlr_scene_tree *tree;
	struct wlr_scene_rect *background;
	struct wsm_session_lock *lock;

	struct wsm_output *output;

	struct wl_list link; // wsm_session_lock::outputs

	struct wl_listener destroy;
	struct wl_listener commit;

	struct wlr_session_lock_surface_v1 *surface;

	struct wl_listener surface_destroy;
	struct wl_listener surface_map;
};

static void wsm_session_lock_destroy(struct wsm_session_lock* lock) {
	struct wsm_session_lock_output *lock_output, *tmp_lock_output;
	wl_list_for_each_safe(lock_output, tmp_lock_output, &lock->outputs, link) {
		wlr_scene_node_destroy(&lock_output->tree->node);
	}

	if (global_server.session_lock.lock == lock) {
		global_server.session_lock.lock = NULL;
	}

	if (!lock->abandoned) {
		wl_list_remove(&lock->destroy.link);
		wl_list_remove(&lock->unlock.link);
		wl_list_remove(&lock->new_surface.link);
	}

	free(lock);
}

static void lock_output_reconfigure(struct wsm_session_lock_output *output) {
	int width = output->output->width;
	int height = output->output->height;

	wlr_scene_rect_set_size(output->background, width, height);

	if (output->surface) {
		wlr_session_lock_surface_v1_configure(output->surface, width, height);
	}
}

static void focus_surface(struct wsm_session_lock *lock,
		struct wlr_surface *focused) {
	lock->focused = focused;

	struct wsm_seat *seat;
	wl_list_for_each(seat, &global_server.input_manager->seats, link) {
		seat_set_focus_surface(seat, focused, false);
	}
}

static void handle_surface_map(struct wl_listener *listener, void *data) {
	struct wsm_session_lock_output *surf = wl_container_of(listener, surf, surface_map);
	if (surf->lock->focused == NULL) {
		focus_surface(surf->lock, surf->surface->surface);
	}
	cursor_rebase_all();
}

static void refocus_output(struct wsm_session_lock_output *output) {
	if (output->lock->focused == output->surface->surface) {
		struct wlr_surface *next_focus = NULL;

		struct wsm_session_lock_output *candidate;
		wl_list_for_each(candidate, &output->lock->outputs, link) {
			if (candidate == output || !candidate->surface) {
				continue;
			}

			if (candidate->surface->surface->mapped) {
				next_focus = candidate->surface->surface;
				break;
			}
		}

		focus_surface(output->lock, next_focus);
	}
}

static void handle_surface_destroy(struct wl_listener *listener, void *data) {
	struct wsm_session_lock_output *output =
		wl_container_of(listener, output, surface_destroy);
	refocus_output(output);

	wsm_assert(output->surface, "Trying to destroy a surface that the lock doesn't think exists");
	output->surface = NULL;
	wl_list_remove(&output->surface_destroy.link);
	wl_list_remove(&output->surface_map.link);
}

static void handle_new_surface(struct wl_listener *listener, void *data) {
	struct wsm_session_lock *lock = wl_container_of(listener, lock, new_surface);
	struct wlr_session_lock_surface_v1 *lock_surface = data;
	struct wsm_output *output = lock_surface->output->data;

	wsm_log(WSM_DEBUG, "new lock layer surface");

	struct wsm_session_lock_output *current_lock_output, *lock_output = NULL;
	wl_list_for_each(current_lock_output, &lock->outputs, link) {
		if (current_lock_output->output == output) {
			lock_output = current_lock_output;
			break;
		}
	}
	wsm_assert(lock_output, "Couldn't find output to lock");
	wsm_assert(!lock_output->surface, "Tried to reassign a surface to an existing output");

	lock_output->surface = lock_surface;

	wlr_scene_subsurface_tree_create(lock_output->tree, lock_surface->surface);

	lock_output->surface_destroy.notify = handle_surface_destroy;
	wl_signal_add(&lock_surface->events.destroy, &lock_output->surface_destroy);
	lock_output->surface_map.notify = handle_surface_map;
	wl_signal_add(&lock_surface->surface->events.map, &lock_output->surface_map);

	lock_output_reconfigure(lock_output);
}

static void handle_unlock(struct wl_listener *listener, void *data) {
	struct wsm_session_lock *lock = wl_container_of(listener, lock, unlock);
	wsm_log(WSM_DEBUG, "session unlocked");

	wsm_session_lock_destroy(lock);

	struct wsm_seat *seat;
	wl_list_for_each(seat, &global_server.input_manager->seats, link) {
		struct wsm_node *previous = seat_get_focus_inactive(seat, &global_server.scene->node);
		if (previous) {
			seat_set_focus(seat, NULL);
			seat_set_focus(seat, previous);
		}
	}

	for (int i = 0; i < global_server.scene->outputs->length; ++i) {
		struct wsm_output *output = global_server.scene->outputs->items[i];
		wsm_arrange_layers(output);
	}
}

static void handle_abandon(struct wl_listener *listener, void *data) {
	struct wsm_session_lock *lock = wl_container_of(listener, lock, destroy);
	wsm_log(WSM_INFO, "session lock abandoned");

	struct wsm_session_lock_output *lock_output;
	wl_list_for_each(lock_output, &lock->outputs, link) {
		wlr_scene_rect_set_color(lock_output->background,
			(float[4]){ 1.f, 0.f, 0.f, 1.f });
	}

	lock->abandoned = true;
	wl_list_remove(&lock->destroy.link);
	wl_list_remove(&lock->unlock.link);
	wl_list_remove(&lock->new_surface.link);
}

static void handle_session_lock(struct wl_listener *listener, void *data) {
	struct wlr_session_lock_v1 *lock = data;
	struct wl_client *client = wl_resource_get_client(lock->resource);

	if (global_server.session_lock.lock) {
		if (global_server.session_lock.lock->abandoned) {
			wsm_log(WSM_INFO, "Replacing abandoned lock");
			wsm_session_lock_destroy(global_server.session_lock.lock);
		} else {
			wsm_log(WSM_ERROR, "Cannot lock an already locked session");
			wlr_session_lock_v1_destroy(lock);
			return;
		}
	}

	struct wsm_session_lock *wsm_lock = calloc(1, sizeof(struct wsm_session_lock));
	if (!wsm_lock) {
		wsm_log(WSM_ERROR, "failed to allocate a session lock object");
		wlr_session_lock_v1_destroy(lock);
		return;
	}

	wl_list_init(&wsm_lock->outputs);
	wsm_lock->session_lock_wlr = lock;

	wsm_log(WSM_DEBUG, "session locked");

	struct wsm_seat *seat;
	wl_list_for_each(seat, &global_server.input_manager->seats, link) {
		seat_unfocus_unless_client(seat, client);
	}

	struct wsm_output *output;
	wl_list_for_each(output, &global_server.scene->all_outputs, link) {
		wsm_session_lock_add_output(wsm_lock, output);
	}

	wsm_lock->new_surface.notify = handle_new_surface;
	wl_signal_add(&lock->events.new_surface, &wsm_lock->new_surface);
	wsm_lock->unlock.notify = handle_unlock;
	wl_signal_add(&lock->events.unlock, &wsm_lock->unlock);
	wsm_lock->destroy.notify = handle_abandon;
	wl_signal_add(&lock->events.destroy, &wsm_lock->destroy);

	wlr_session_lock_v1_send_locked(lock);
	global_server.session_lock.lock = wsm_lock;
}

static void handle_session_lock_destroy(struct wl_listener *listener, void *data) {
	if (global_server.session_lock.lock) {
		wsm_session_lock_destroy(global_server.session_lock.lock);
	}

	wl_list_remove(&global_server.session_lock.new_lock.link);
	wl_list_remove(&global_server.session_lock.manager_destroy.link);

	global_server.session_lock.manager = NULL;
}

static void wsm_session_lock_output_destroy(struct wsm_session_lock_output *output) {
	if (output->surface) {
		refocus_output(output);
		wl_list_remove(&output->surface_destroy.link);
		wl_list_remove(&output->surface_map.link);
	}

	wl_list_remove(&output->commit.link);
	wl_list_remove(&output->destroy.link);
	wl_list_remove(&output->link);

	free(output);
}

static void lock_node_handle_destroy(struct wl_listener *listener, void *data) {
	struct wsm_session_lock_output *output =
		wl_container_of(listener, output, destroy);
	wsm_session_lock_output_destroy(output);
}

static void lock_output_handle_commit(struct wl_listener *listener, void *data) {
	struct wlr_output_event_commit *event = data;
	struct wsm_session_lock_output *output =
		wl_container_of(listener, output, commit);
	if (event->state->committed & (
			WLR_OUTPUT_STATE_MODE |
			WLR_OUTPUT_STATE_SCALE |
			WLR_OUTPUT_STATE_TRANSFORM)) {
		lock_output_reconfigure(output);
	}
}

static struct wsm_session_lock_output *session_lock_output_create(
	struct wsm_session_lock *lock, struct wsm_output *output) {
	struct wsm_session_lock_output *lock_output = calloc(1, sizeof(struct wsm_session_lock_output));
	if (!lock_output) {
		wsm_log(WSM_ERROR, "failed to allocate a session lock output");
		return NULL;
	}

	struct wlr_scene_tree *tree = wlr_scene_tree_create(output->layers.session_lock);
	if (!tree) {
		wsm_log(WSM_ERROR, "failed to allocate a session lock output scene tree");
		free(lock_output);
		return NULL;
	}

	struct wlr_scene_rect *background = wlr_scene_rect_create(tree, 0, 0, (float[4]){
		lock->abandoned ? 1.f : 0.f, 0.f, 0.f, 1.f,}
	);
	if (!background) {
		wsm_log(WSM_ERROR, "failed to allocate a session lock output scene background");
		wlr_scene_node_destroy(&tree->node);
		free(lock_output);
		return NULL;
	}

	lock_output->output = output;
	lock_output->tree = tree;
	lock_output->background = background;
	lock_output->lock = lock;

	lock_output->destroy.notify = lock_node_handle_destroy;
	wl_signal_add(&tree->node.events.destroy, &lock_output->destroy);

	lock_output->commit.notify = lock_output_handle_commit;
	wl_signal_add(&output->wlr_output->events.commit, &lock_output->commit);

	lock_output_reconfigure(lock_output);

	wl_list_insert(&lock->outputs, &lock_output->link);

	return lock_output;
}

void wsm_session_lock_init(void) {
	global_server.session_lock.manager = wlr_session_lock_manager_v1_create(global_server.wl_display);

	global_server.session_lock.new_lock.notify = handle_session_lock;
	wl_signal_add(&global_server.session_lock.manager->events.new_lock,
		&global_server.session_lock.new_lock);

	global_server.session_lock.manager_destroy.notify = handle_session_lock_destroy;
	wl_signal_add(&global_server.session_lock.manager->events.destroy,
		&global_server.session_lock.manager_destroy);
}

void wsm_session_lock_add_output(struct wsm_session_lock *lock,
	struct wsm_output *output) {
	struct wsm_session_lock_output *lock_output =
		session_lock_output_create(lock, output);

	if (!lock_output) {
		wsm_log(WSM_ERROR, "aborting: failed to allocate a lock output");
		abort();
	}
}

bool wsm_session_lock_has_surface(struct wsm_session_lock *lock,
	struct wlr_surface *surface) {
	struct wsm_session_lock_output *lock_output;
	wl_list_for_each(lock_output, &lock->outputs, link) {
		if (lock_output->surface && lock_output->surface->surface == surface) {
			return true;
		}
	}

	return false;
}
