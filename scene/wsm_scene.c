#include "scene/wsm_scene.h"
#include "scene/wsm_scene_tree.h"
#include "scene/wsm_scene_buffer.h"
#include "util/wsm_env.h"
#include "scene/wsm_scene_output.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <wlr/config.h>
#include <wlr/backend.h>
#include <wlr/render/swapchain.h>
#include <wlr/render/drm_syncobj.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_color_management_v1.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_damage_ring.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_linux_dmabuf_v1.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/util/log.h>
#include <wlr/util/region.h>
#include <wlr/util/transform.h>
#include <wlr/render/color.h>

struct wsm_scene *wsm_scene_create(void) {
	struct wsm_scene *scene = calloc(1, sizeof(*scene));
	if (scene == NULL) {
		return NULL;
	}

	scene->tree = wsm_root_scene_tree_create(scene);
	if (scene->tree == NULL) {
		free(scene);
		return NULL;
	}
	scene->tree->node.data = scene;

	wl_list_init(&scene->outputs);
	wl_list_init(&scene->linux_dmabuf_v1_destroy.link);
	wl_list_init(&scene->gamma_control_manager_v1_destroy.link);
	wl_list_init(&scene->gamma_control_manager_v1_set_gamma.link);

	const char *debug_damage_options[] = {
		"none",
		"rerender",
		"highlight",
		NULL
	};

	scene->debug_damage_option = env_parse_switch("WLR_SCENE_DEBUG_DAMAGE", debug_damage_options);
	scene->direct_scanout = !env_parse_bool("WLR_SCENE_DISABLE_DIRECT_SCANOUT");
	scene->calculate_visibility = !env_parse_bool("WLR_SCENE_DISABLE_VISIBILITY");
	scene->highlight_transparent_region = env_parse_bool("WLR_SCENE_HIGHLIGHT_TRANSPARENT_REGION");

	return scene;
}

void wsm_scene_destroy(struct wsm_scene *scene) {
	if (scene == NULL) {
		return;
	}

	wsm_scene_node_destroy(&scene->tree->node);
	free(scene);
}

static void scene_handle_linux_dmabuf_v1_destroy(struct wl_listener *listener,
		void *data) {
	struct wsm_scene *scene =
		wl_container_of(listener, scene, linux_dmabuf_v1_destroy);
	wl_list_remove(&scene->linux_dmabuf_v1_destroy.link);
	wl_list_init(&scene->linux_dmabuf_v1_destroy.link);
	scene->linux_dmabuf_v1 = NULL;
}

void wsm_scene_set_linux_dmabuf_v1(struct wsm_scene *scene,
		struct wlr_linux_dmabuf_v1 *linux_dmabuf_v1) {
	assert(scene->linux_dmabuf_v1 == NULL);
	scene->linux_dmabuf_v1 = linux_dmabuf_v1;
	scene->linux_dmabuf_v1_destroy.notify = scene_handle_linux_dmabuf_v1_destroy;
	wl_signal_add(&linux_dmabuf_v1->events.destroy, &scene->linux_dmabuf_v1_destroy);
}

static void scene_handle_gamma_control_manager_v1_set_gamma(struct wl_listener *listener,
		void *data) {
	const struct wlr_gamma_control_manager_v1_set_gamma_event *event = data;
	struct wsm_scene *scene =
		wl_container_of(listener, scene, gamma_control_manager_v1_set_gamma);
	struct wsm_scene_output *output = wsm_scene_get_scene_output(scene, event->output);
	if (!output) {
		// this scene might not own this output.
		return;
	}

	output->gamma_lut_changed = true;
	output->gamma_lut = event->control;
	wlr_color_transform_unref(output->gamma_lut_color_transform);
	output->gamma_lut_color_transform = wlr_gamma_control_v1_get_color_transform(event->control);
	wlr_output_schedule_frame(output->output);
}

static void scene_handle_gamma_control_manager_v1_destroy(struct wl_listener *listener,
		void *data) {
	struct wsm_scene *scene =
		wl_container_of(listener, scene, gamma_control_manager_v1_destroy);
	wl_list_remove(&scene->gamma_control_manager_v1_destroy.link);
	wl_list_init(&scene->gamma_control_manager_v1_destroy.link);
	wl_list_remove(&scene->gamma_control_manager_v1_set_gamma.link);
	wl_list_init(&scene->gamma_control_manager_v1_set_gamma.link);
	scene->gamma_control_manager_v1 = NULL;

	struct wsm_scene_output *output;
	wl_list_for_each(output, &scene->outputs, link) {
		output->gamma_lut_changed = false;
		output->gamma_lut = NULL;
		wlr_color_transform_unref(output->gamma_lut_color_transform);
		output->gamma_lut_color_transform = NULL;
	}
}

void wsm_scene_set_gamma_control_manager_v1(struct wsm_scene *scene,
	    struct wlr_gamma_control_manager_v1 *gamma_control) {
	assert(scene->gamma_control_manager_v1 == NULL);
	scene->gamma_control_manager_v1 = gamma_control;

	scene->gamma_control_manager_v1_destroy.notify =
		scene_handle_gamma_control_manager_v1_destroy;
	wl_signal_add(&gamma_control->events.destroy, &scene->gamma_control_manager_v1_destroy);
	scene->gamma_control_manager_v1_set_gamma.notify =
		scene_handle_gamma_control_manager_v1_set_gamma;
	wl_signal_add(&gamma_control->events.set_gamma, &scene->gamma_control_manager_v1_set_gamma);
}

static void scene_handle_color_manager_v1_destroy(struct wl_listener *listener, void *data) {
	struct wsm_scene *scene = wl_container_of(listener, scene, color_manager_v1_destroy);
	wl_list_remove(&scene->color_manager_v1_destroy.link);
	wl_list_init(&scene->color_manager_v1_destroy.link);
	scene->color_manager_v1 = NULL;
}

void wsm_scene_set_color_manager_v1(struct wsm_scene *scene, struct wlr_color_manager_v1 *manager) {
	assert(scene->color_manager_v1 == NULL);
	scene->color_manager_v1 = manager;

	scene->color_manager_v1_destroy.notify = scene_handle_color_manager_v1_destroy;
	wl_signal_add(&manager->events.destroy, &scene->color_manager_v1_destroy);
}

void wsm_scene_timer_finish(struct wsm_scene_timer *timer) {
	if (timer->render_timer) {
		wlr_render_timer_destroy(timer->render_timer);
	}
}

struct wsm_scene *wsm_scene_node_get_root(struct wsm_scene_node *node) {
	return node->scene;
}
