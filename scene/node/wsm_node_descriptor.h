#ifndef WSM_NODE_DESCRIPTOR_H
#define WSM_NODE_DESCRIPTOR_H

#include <wayland-server-core.h>

struct wlr_scene_node;

struct wsm_view;

/**
 * @brief Enum representing the types of scene descriptors in the WSM
 */
enum wsm_scene_descriptor_type {
	WSM_SCENE_DESC_BUFFER_TIMER, /**< Descriptor for buffer timer */
	WSM_SCENE_DESC_NON_INTERACTIVE, /**< Descriptor for non-interactive elements */
	WSM_SCENE_DESC_CONTAINER, /**< Descriptor for container elements */
	WSM_SCENE_DESC_VIEW, /**< Descriptor for view elements */
	WSM_SCENE_DESC_LAYER_SHELL, /**< Descriptor for layer shell elements */
	WSM_SCENE_DESC_XWAYLAND_UNMANAGED, /**< Descriptor for unmanaged XWayland elements */
	WSM_SCENE_DESC_POPUP, /**< Descriptor for popup elements */
	WSM_SCENE_DESC_DRAG_ICON, /**< Descriptor for drag icon elements */
};

/**
 * @brief Assigns a descriptor to a scene node
 * @param node Pointer to the wlr_scene_node to which the descriptor will be assigned
 * @param type The type of the descriptor to assign
 * @param data Pointer to additional data associated with the descriptor
 * @return true if the assignment was successful, false otherwise
 */
bool wsm_scene_descriptor_assign(struct wlr_scene_node *node,
	enum wsm_scene_descriptor_type type, void *data);

/**
 * @brief Tries to retrieve a descriptor from a scene node
 * @param node Pointer to the wlr_scene_node from which to retrieve the descriptor
 * @param type The type of the descriptor to retrieve
 * @return Pointer to the associated data if found, NULL otherwise
 */
void *wsm_scene_descriptor_try_get(struct wlr_scene_node *node,
	enum wsm_scene_descriptor_type type);

/**
 * @brief Destroys a descriptor associated with a scene node
 * @param node Pointer to the wlr_scene_node from which the descriptor will be destroyed
 * @param type The type of the descriptor to destroy
 */
void wsm_scene_descriptor_destroy(struct wlr_scene_node *node,
	enum wsm_scene_descriptor_type type);

#endif
