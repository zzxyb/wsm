#ifndef WSM_TABLET_H
#define WSM_TABLET_H

#include <wayland-server-core.h>

struct wlr_surface;
struct wlr_tablet_pad;
struct wlr_tablet_tool;
struct wlr_tablet_v2_tablet;
struct wlr_tablet_v2_tablet_tool;
struct wlr_tablet_v2_tablet_pad;
struct wlr_tablet_manager_v2;

struct wsm_seat;
struct wsm_seat_device;

/**
 * @brief Structure representing a tablet in the WSM
 */
struct wsm_tablet {
	struct wl_list link; /**< Link for managing a list of tablets */
	struct wsm_seat_device *seat_device; /**< Pointer to the associated WSM seat device */
	struct wlr_tablet_v2_tablet *tablet_v2; /**< Pointer to the WLR tablet instance */
	struct wlr_tablet_manager_v2 *tablet_manager_v2; /**< Pointer to the WLR tablet manager instance */
};

/**
 * @brief Enumeration of tablet tool modes
 */
enum wsm_tablet_tool_mode {
	WSM_TABLET_TOOL_MODE_ABSOLUTE, /**< Absolute mode for tablet tool */
	WSM_TABLET_TOOL_MODE_RELATIVE, /**< Relative mode for tablet tool */
};

/**
 * @brief Structure representing a tablet tool in the WSM
 */
struct wsm_tablet_tool {
	struct wl_listener set_cursor; /**< Listener for setting the cursor */
	struct wl_listener tool_destroy; /**< Listener for tool destruction events */

	struct wsm_seat *seat; /**< Pointer to the associated WSM seat */
	struct wsm_tablet *tablet; /**< Pointer to the associated WSM tablet */
	struct wlr_tablet_v2_tablet_tool *tablet_v2_tool; /**< Pointer to the WLR tablet tool instance */

	double tilt_x, tilt_y; /**< Tilt angles of the tablet tool */
	enum wsm_tablet_tool_mode mode; /**< Current mode of the tablet tool */
};

/**
 * @brief Structure representing a tablet pad in the WSM
 */
struct wsm_tablet_pad {
	struct wl_listener attach; /**< Listener for attach events */
	struct wl_listener button; /**< Listener for button events */
	struct wl_listener ring; /**< Listener for ring events */
	struct wl_listener strip; /**< Listener for strip events */

	struct wl_listener surface_destroy; /**< Listener for surface destruction events */
	struct wl_listener tablet_destroy; /**< Listener for tablet destruction events */

	struct wl_list link; /**< Link for managing a list of tablet pads */
	struct wsm_seat_device *seat_device; /**< Pointer to the associated WSM seat device */
	struct wsm_tablet *tablet; /**< Pointer to the associated WSM tablet */
	struct wlr_tablet_pad *tablet_pad_wlr; /**< Pointer to the WLR tablet pad instance */
	struct wlr_tablet_v2_tablet_pad *tablet_v2_pad; /**< Pointer to the WLR V2 tablet pad instance */

	struct wlr_surface *current_surface; /**< Pointer to the current surface associated with the tablet pad */
};

/**
 * @brief Creates a new wsm_tablet instance
 * @param seat Pointer to the WSM seat associated with the tablet
 * @param device Pointer to the WSM seat device associated with the tablet
 * @return Pointer to the newly created wsm_tablet instance
 */
struct wsm_tablet *wsm_tablet_create(struct wsm_seat *seat,
	struct wsm_seat_device *device);

/**
 * @brief Configures the specified wsm_tablet instance
 * @param tablet Pointer to the wsm_tablet instance to configure
 */
void wsm_configure_tablet(struct wsm_tablet *tablet);

/**
 * @brief Destroys the specified wsm_tablet instance
 * @param tablet Pointer to the wsm_tablet instance to destroy
 */
void wsm_tablet_destroy(struct wsm_tablet *tablet);

/**
 * @brief Configures the specified tablet tool for the given tablet
 * @param tablet Pointer to the wsm_tablet instance
 * @param wlr_tool Pointer to the WLR tablet tool to configure
 */
void wsm_tablet_tool_configure(struct wsm_tablet *tablet,
	struct wlr_tablet_tool *wlr_tool);

/**
 * @brief Creates a new wsm_tablet_pad instance
 * @param seat Pointer to the WSM seat associated with the tablet pad
 * @param device Pointer to the WSM seat device associated with the tablet pad
 * @return Pointer to the newly created wsm_tablet_pad instance
 */
struct wsm_tablet_pad *wsm_tablet_pad_create(struct wsm_seat *seat,
	struct wsm_seat_device *device);

/**
 * @brief Configures the specified wsm_tablet_pad instance
 * @param tablet_pad Pointer to the wsm_tablet_pad instance to configure
 */
void wsm_configure_tablet_pad(struct wsm_tablet_pad *tablet_pad);

/**
 * @brief Destroys the specified wsm_tablet_pad instance
 * @param tablet_pad Pointer to the wsm_tablet_pad instance to destroy
 */
void wsm_tablet_pad_destroy(struct wsm_tablet_pad *tablet_pad);

/**
 * @brief Sets focus to the specified surface for the tablet pad
 * @param tablet_pad Pointer to the wsm_tablet_pad instance
 * @param surface Pointer to the WLR surface to focus
 */
void wsm_tablet_pad_set_focus(struct wsm_tablet_pad *tablet_pad,
	struct wlr_surface *surface);

#endif
