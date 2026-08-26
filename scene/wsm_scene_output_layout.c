#include "scene/wsm_scene_output_layout.h"
#include "scene/wsm_scene_output.h"
#include "scene/wsm_scene.h"
#include "scene/wsm_scene_tree.h"

#include <stdlib.h>
#include <assert.h>

#include <wlr/types/wlr_output_layout.h>

static void scene_output_layout_output_destroy(
		struct wsm_scene_output_layout_output *solo) {
	wl_list_remove(&solo->layout_output_destroy.link);
	wl_list_remove(&solo->scene_output_destroy.link);
	wl_list_remove(&solo->link);
	free(solo);
}

static void scene_output_layout_output_handle_layout_output_destroy(
		struct wl_listener *listener, void *data) {
	struct wsm_scene_output_layout_output *solo =
		wl_container_of(listener, solo, layout_output_destroy);
	scene_output_layout_output_destroy(solo);
}

static void scene_output_layout_output_handle_scene_output_destroy(
		struct wl_listener *listener, void *data) {
	struct wsm_scene_output_layout_output *solo =
		wl_container_of(listener, solo, scene_output_destroy);
	scene_output_layout_output_destroy(solo);
}

static void scene_output_layout_destroy(struct wsm_scene_output_layout *sol) {
	struct wsm_scene_output_layout_output *solo, *tmp;
	wl_list_for_each_safe(solo, tmp, &sol->outputs, link) {
		scene_output_layout_output_destroy(solo);
	}
	wl_list_remove(&sol->layout_change.link);
	wl_list_remove(&sol->layout_destroy.link);
	wl_list_remove(&sol->scene_destroy.link);
	free(sol);
}

static void scene_output_layout_handle_layout_change(
		struct wl_listener *listener, void *data) {
	struct wsm_scene_output_layout *sol =
		wl_container_of(listener, sol, layout_change);

	struct wsm_scene_output_layout_output *solo;
	wl_list_for_each(solo, &sol->outputs, link) {
		wsm_scene_output_set_position(solo->scene_output,
			solo->layout_output->x, solo->layout_output->y);
	}
}

void wsm_scene_output_layout_add_output(struct wsm_scene_output_layout *sol,
		struct wlr_output_layout_output *lo, struct wsm_scene_output *so) {
	assert(lo->output == so->output);

	struct wsm_scene_output_layout_output *solo;
	wl_list_for_each(solo, &sol->outputs, link) {
		if (solo->scene_output == so) {
			return;
		}
	}

	solo = calloc(1, sizeof(*solo));
	if (solo == NULL) {
		return;
	}

	solo->scene_output = so;
	solo->layout_output = lo;

	solo->layout_output_destroy.notify =
		scene_output_layout_output_handle_layout_output_destroy;
	wl_signal_add(&lo->events.destroy, &solo->layout_output_destroy);

	solo->scene_output_destroy.notify =
		scene_output_layout_output_handle_scene_output_destroy;
	wl_signal_add(&solo->scene_output->events.destroy,
		&solo->scene_output_destroy);

	wl_list_insert(&sol->outputs, &solo->link);

	wsm_scene_output_set_position(solo->scene_output, lo->x, lo->y);
}

static void scene_output_layout_handle_layout_destroy(
		struct wl_listener *listener, void *data) {
	struct wsm_scene_output_layout *sol =
		wl_container_of(listener, sol, layout_destroy);
	scene_output_layout_destroy(sol);
}

static void scene_output_layout_handle_scene_destroy(
		struct wl_listener *listener, void *data) {
	struct wsm_scene_output_layout *sol =
		wl_container_of(listener, sol, scene_destroy);
	scene_output_layout_destroy(sol);
}

struct wsm_scene_output_layout *wsm_scene_attach_output_layout(struct wsm_scene *scene,
		struct wlr_output_layout *output_layout) {
	struct wsm_scene_output_layout *sol = calloc(1, sizeof(*sol));
	if (sol == NULL) {
		return NULL;
	}

	sol->scene = scene;
	sol->layout = output_layout;

	wl_list_init(&sol->outputs);

	sol->layout_destroy.notify = scene_output_layout_handle_layout_destroy;
	wl_signal_add(&output_layout->events.destroy, &sol->layout_destroy);

	sol->layout_change.notify = scene_output_layout_handle_layout_change;
	wl_signal_add(&output_layout->events.change, &sol->layout_change);

	sol->scene_destroy.notify = scene_output_layout_handle_scene_destroy;
	wl_signal_add(&scene->tree->node.events.destroy, &sol->scene_destroy);

	return sol;
}
