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

#ifndef WSM_NODE_H
#define WSM_NODE_H

#include "wsm_list.h"

#include <stddef.h>
#include <stdbool.h>

#include <wayland-server-core.h>

struct wlr_box;
struct wlr_scene_tree;

struct wsm_root;
struct wsm_output;
struct wsm_workspace;
struct wsm_container;
struct wsm_transaction_instruction;

enum wsm_node_type {
	N_ROOT,
	N_OUTPUT,
	N_WORKSPACE,
	N_CONTAINER,
};

struct wsm_node {
	struct {
		struct wl_signal destroy;
	} events;

	union {
		struct wsm_root *wsm_root;
		struct wsm_output *wsm_output;
		struct wsm_workspace *wsm_workspace;
		struct wsm_container *wsm_container;
	};

	size_t id;
	size_t ntxnrefs;
	struct wsm_transaction_instruction *instruction;

	enum wsm_node_type type;
	bool destroying;
	bool dirty;
};

void node_init(struct wsm_node *node, enum wsm_node_type type, void *thing);
const char *node_type_to_str(enum wsm_node_type type);
void node_set_dirty(struct wsm_node *node);
bool node_is_view(struct wsm_node *node);
char *node_get_name(struct wsm_node *node);
void node_get_box(struct wsm_node *node, struct wlr_box *box);
struct wsm_output *node_get_output(struct wsm_node *node);
enum wsm_container_layout node_get_layout(struct wsm_node *node);
struct wsm_node *node_get_parent(struct wsm_node *node);
struct wsm_list *node_get_children(struct wsm_node *node);
void scene_node_disown_children(struct wlr_scene_tree *tree);
bool node_has_ancestor(struct wsm_node *node, struct wsm_node *ancestor);
struct wlr_scene_tree *alloc_scene_tree(struct wlr_scene_tree *parent,
	bool *failed);

#endif
