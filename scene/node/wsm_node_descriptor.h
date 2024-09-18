#ifndef WSM_NODE_DESCRIPTOR_H
#define WSM_NODE_DESCRIPTOR_H

#include <wayland-server-core.h>

struct wlr_scene_node;

struct wsm_view;

enum wsm_scene_descriptor_type {
	WSM_SCENE_DESC_BUFFER_TIMER,
	WSM_SCENE_DESC_NON_INTERACTIVE,
	WSM_SCENE_DESC_CONTAINER,
	WSM_SCENE_DESC_VIEW,
	WSM_SCENE_DESC_LAYER_SHELL,
	WSM_SCENE_DESC_XWAYLAND_UNMANAGED,
	WSM_SCENE_DESC_POPUP,
	WSM_SCENE_DESC_DRAG_ICON,
};

bool wsm_scene_descriptor_assign(struct wlr_scene_node *node,
	enum wsm_scene_descriptor_type type, void *data);

void *wsm_scene_descriptor_try_get(struct wlr_scene_node *node,
	enum wsm_scene_descriptor_type type);

void wsm_scene_descriptor_destroy(struct wlr_scene_node *node,
	enum wsm_scene_descriptor_type type);

#endif
