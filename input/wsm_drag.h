#ifndef WSM_DRAG_H
#define WSM_DRAG_H

#include <wayland-server-core.h>

struct wsm_seat;

struct wlr_drag;
struct wlr_scene_node;

/**
 * @brief Structure representing a drag operation
 */
struct wsm_drag {
	struct wl_listener destroy; /**< Listener for destruction events */
	struct wsm_seat *seat; /**< Pointer to the associated WSM seat */
	struct wlr_drag *drag; /**< Pointer to the WLR drag instance */
};

/**
 * @brief Creates a new drag operation
 * @param seat Pointer to the wsm_seat instance
 * @param wlr_drag Pointer to the WLR drag instance
 * @return Pointer to the newly created wsm_drag instance
 */
struct wsm_drag *wsm_drag_create(struct wsm_seat *seat, struct wlr_drag *wlr_drag);

/**
 * @brief Destroys a drag operation
 * @param drag Pointer to the wsm_drag instance to be destroyed
 */
void wsm_drag_destroy(struct wsm_drag *drag);

/**
 * @brief Updates the position of drag icons for the specified seat
 * @param seat Pointer to the wsm_seat instance
 */
void wsm_drag_icons_update_position(struct wsm_seat *seat);

/**
 * @brief Updates the position of a drag icon node for the specified seat
 * @param seat Pointer to the wsm_seat instance
 * @param node Pointer to the wlr_scene_node instance representing the drag icon
 */
void wsm_drag_icon_node_update_position(struct wsm_seat *seat,
	struct wlr_scene_node *node);

#endif
