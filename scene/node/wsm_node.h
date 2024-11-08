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

/**
 * @brief Enum representing the types of nodes in the WSM
 */
enum wsm_node_type {
	N_ROOT, /**< Root node type */
	N_OUTPUT, /**< Output node type */
	N_WORKSPACE, /**< Workspace node type */
	N_CONTAINER, /**< Container node type */
};

/**
 * @brief Structure representing a node in the WSM
 */
struct wsm_node {
	struct {
		struct wl_signal destroy; /**< Signal emitted when the node is destroyed */
	} events;

	union {
		struct wsm_root *root; /**< Pointer to the root node */
		struct wsm_output *output; /**< Pointer to the output node */
		struct wsm_workspace *workspace; /**< Pointer to the workspace node */
		struct wsm_container *container; /**< Pointer to the container node */
	};

	size_t id; /**< Unique identifier for the node */
	size_t ntxnrefs; /**< Number of transaction references for the node */
	struct wsm_transaction_instruction *instruction; /**< Pointer to the associated transaction instruction */

	enum wsm_node_type type; /**< Type of the node */
	bool destroying; /**< Flag indicating if the node is being destroyed */
	bool dirty; /**< Flag indicating if the node is marked as dirty */
};

/**
 * @brief Initializes a wsm_node instance
 * @param node Pointer to the wsm_node to be initialized
 * @param type Type of the node to be initialized
 * @param thing Pointer to the associated object (e.g., root, output, workspace, container)
 */
void node_init(struct wsm_node *node, enum wsm_node_type type, void *thing);

/**
 * @brief Converts a node type to its string representation
 * @param type The wsm_node_type to be converted
 * @return Pointer to the string representation of the node type
 */
const char *node_type_to_str(enum wsm_node_type type);

/**
 * @brief Marks the specified node as dirty
 * @param node Pointer to the wsm_node to be marked as dirty
 */
void node_set_dirty(struct wsm_node *node);

/**
 * @brief Checks if the specified node is a view
 * @param node Pointer to the wsm_node to be checked
 * @return true if the node is a view, false otherwise
 */
bool node_is_view(struct wsm_node *node);

/**
 * @brief Retrieves the name of the specified node
 * @param node Pointer to the wsm_node whose name is to be retrieved
 * @return Pointer to the name string of the node
 */
char *node_get_name(struct wsm_node *node);

/**
 * @brief Retrieves the bounding box of the specified node
 * @param node Pointer to the wsm_node whose box is to be retrieved
 * @param box Pointer to the wlr_box to be populated with the dimensions
 */
void node_get_box(struct wsm_node *node, struct wlr_box *box);

/**
 * @brief Retrieves the output associated with the specified node
 * @param node Pointer to the wsm_node whose output is to be retrieved
 * @return Pointer to the associated wsm_output
 */
struct wsm_output *node_get_output(struct wsm_node *node);

/**
 * @brief Retrieves the layout of the container associated with the specified node
 * @param node Pointer to the wsm_node whose layout is to be retrieved
 * @return The layout of the associated container
 */
enum wsm_container_layout node_get_layout(struct wsm_node *node);

/**
 * @brief Retrieves the parent node of the specified node
 * @param node Pointer to the wsm_node whose parent is to be retrieved
 * @return Pointer to the parent wsm_node
 */
struct wsm_node *node_get_parent(struct wsm_node *node);

/**
 * @brief Retrieves the children of the specified node
 * @param node Pointer to the wsm_node whose children are to be retrieved
 * @return Pointer to the list of child nodes
 */
struct wsm_list *node_get_children(struct wsm_node *node);

/**
 * @brief Disowns the children of the specified scene tree
 * @param tree Pointer to the wlr_scene_tree whose children are to be disowned
 */
void scene_node_disown_children(struct wlr_scene_tree *tree);

/**
 * @brief Checks if the specified node has a given ancestor
 * @param node Pointer to the wsm_node to be checked
 * @param ancestor Pointer to the potential ancestor node
 * @return true if the node has the ancestor, false otherwise
 */
bool node_has_ancestor(struct wsm_node *node, struct wsm_node *ancestor);

/**
 * @brief Allocates a new scene tree
 * @param parent Pointer to the parent scene tree
 * @param failed Pointer to a boolean indicating if the allocation failed
 * @return Pointer to the newly allocated wlr_scene_tree
 */
struct wlr_scene_tree *alloc_scene_tree(struct wlr_scene_tree *parent,
	bool *failed);

#endif
