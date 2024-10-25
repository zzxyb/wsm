#include "wsm_node_descriptor.h"
#include "wsm_log.h"

#include <stdlib.h>

#include <wlr/types/wlr_scene.h>

struct scene_descriptor {
	struct wlr_addon addon;
	void *data;
};

static void descriptor_destroy(struct scene_descriptor *desc) {
	wlr_addon_finish(&desc->addon);
	free(desc);
}

static void addon_handle_destroy(struct wlr_addon *addon) {
	struct scene_descriptor *desc = wl_container_of(addon, desc, addon);
	descriptor_destroy(desc);
}

static const struct wlr_addon_interface addon_interface = {
	.name = "wsm_scene_descriptor",
	.destroy = addon_handle_destroy,
};

static struct scene_descriptor *scene_node_get_descriptor(
		struct wlr_scene_node *node, enum wsm_scene_descriptor_type type) {
	struct wlr_addon *addon = wlr_addon_find(&node->addons, (void *)type, &addon_interface);
	if (!addon) {
		return NULL;
	}

	struct scene_descriptor *desc = wl_container_of(addon, desc, addon);
	return desc;
}

bool wsm_scene_descriptor_assign(struct wlr_scene_node *node,
		enum wsm_scene_descriptor_type type, void *data) {
	struct scene_descriptor *desc = calloc(1, sizeof(*desc));
	if (!desc) {
		wsm_log(WSM_ERROR, "Could not create scene_descriptor: allocation failed!");
		return false;
	}

	wlr_addon_init(&desc->addon, &node->addons, (void *)type, &addon_interface);
	desc->data = data;
	return true;
}

void *wsm_scene_descriptor_try_get(struct wlr_scene_node *node,
		enum wsm_scene_descriptor_type type) {
	struct scene_descriptor *desc = scene_node_get_descriptor(node, type);
	if (!desc) {
		return NULL;
	}

	return desc->data;
}

void wsm_scene_descriptor_destroy(struct wlr_scene_node *node,
		enum wsm_scene_descriptor_type type) {
	struct scene_descriptor *desc = scene_node_get_descriptor(node, type);
	descriptor_destroy(desc);
}
