#include "wsm_node.h"
#include "wsm_log.h"
#include "wsm_scene.h"
#include "wsm_server.h"
#include "wsm_output.h"
#include "wsm_workspace.h"
#include "wsm_container.h"

#include <wlr/types/wlr_scene.h>

void node_init(struct wsm_node *node, enum wsm_node_type type, void *thing) {
	static size_t next_id = 1;
	node->id = next_id++;
	node->type = type;
	node->wsm_root = thing;
	wl_signal_init(&node->events.destroy);
}

const char *node_type_to_str(enum wsm_node_type type) {
	switch (type) {
	case N_ROOT:
		return "root";
	case N_OUTPUT:
		return "output";
	case N_WORKSPACE:
		return "workspace";
	case N_CONTAINER:
		return "container";
	}
	return "";
}

void node_set_dirty(struct wsm_node *node) {
	if (node->dirty) {
		return;
	}
	node->dirty = true;
	list_add(global_server.dirty_nodes, node);
}

bool node_is_view(struct wsm_node *node) {
	return node->type == N_CONTAINER && node->wsm_container->view;
}

char *node_get_name(struct wsm_node *node) {
	switch (node->type) {
	case N_ROOT:
		return "root";
	case N_OUTPUT:
		return node->wsm_output->wlr_output->name;
	case N_WORKSPACE:
		return node->wsm_workspace->name;
	case N_CONTAINER:
		return node->wsm_container->title;
	}
	return NULL;
}

void node_get_box(struct wsm_node *node, struct wlr_box *box) {
	switch (node->type) {
	case N_ROOT:
		root_get_box(global_server.wsm_scene, box);
		break;
	case N_OUTPUT:
		output_get_box(node->wsm_output, box);
		break;
	case N_WORKSPACE:
		workspace_get_box(node->wsm_workspace, box);
		break;
	case N_CONTAINER:
		container_get_box(node->wsm_container, box);
		break;
	}
}

struct wsm_output *node_get_output(struct wsm_node *node) {
	switch (node->type) {
	case N_CONTAINER: {
		struct wsm_workspace *ws = node->wsm_container->pending.workspace;
		return ws ? ws->output : NULL;
	}
	case N_WORKSPACE:
		return node->wsm_workspace->output;
	case N_OUTPUT:
		return node->wsm_output;
	case N_ROOT:
		return NULL;
	}
	return NULL;
}

enum wsm_container_layout node_get_layout(struct wsm_node *node) {
	switch (node->type) {
	case N_CONTAINER:
		return node->wsm_container->pending.layout;
	case N_WORKSPACE:
		return node->wsm_workspace->layout;
	case N_OUTPUT:
	case N_ROOT:
		return L_NONE;
	}
	return L_NONE;
}

struct wsm_node *node_get_parent(struct wsm_node *node) {
	switch (node->type) {
	case N_CONTAINER: {
		struct wsm_container *con = node->wsm_container;
		if (con->pending.parent) {
			return &con->pending.parent->node;
		}
		if (con->pending.workspace) {
			return &con->pending.workspace->node;
		}
	}
	return NULL;
	case N_WORKSPACE: {
		struct wsm_workspace *ws = node->wsm_workspace;
		if (ws->output) {
			return &ws->output->node;
		}
	}
		return NULL;
	case N_OUTPUT:
		return &global_server.wsm_scene->node;
	case N_ROOT:
		return NULL;
	}
	return NULL;
}

struct wsm_list *node_get_children(struct wsm_node *node) {
	switch (node->type) {
	case N_CONTAINER:
		return node->wsm_container->pending.children;
	case N_WORKSPACE:
		return node->wsm_workspace->tiling;
	case N_OUTPUT:
	case N_ROOT:
		return NULL;
	}
	return NULL;
}

void scene_node_disown_children(struct wlr_scene_tree *tree) {
	if (!tree) {
		return;
	}

	struct wlr_scene_node *child, *tmp_child;
	wl_list_for_each_safe(child, tmp_child, &tree->children, link) {
		wlr_scene_node_reparent(child, global_server.wsm_scene->staging);
	}
}

bool node_has_ancestor(struct wsm_node *node, struct wsm_node *ancestor) {
	if (ancestor->type == N_ROOT && node->type == N_CONTAINER &&
		node->wsm_container->pending.fullscreen_mode == FULLSCREEN_GLOBAL) {
		return true;
	}
	struct wsm_node *parent = node_get_parent(node);
	while (parent) {
		if (parent == ancestor) {
			return true;
		}
		if (ancestor->type == N_ROOT && parent->type == N_CONTAINER &&
			parent->wsm_container->pending.fullscreen_mode == FULLSCREEN_GLOBAL) {
			return true;
		}
		parent = node_get_parent(parent);
	}
	return false;
}

struct wlr_scene_tree *alloc_scene_tree(struct wlr_scene_tree *parent,
		bool *failed) {
	if (*failed) {
		return NULL;
	}

	struct wlr_scene_tree *tree = wlr_scene_tree_create(parent);
	if (!tree) {
		wsm_log(WSM_ERROR, "Failed to allocate a scene node");
		*failed = true;
	}

	return tree;
}
