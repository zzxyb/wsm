#ifndef WSM_SWITCH_H
#define WSM_SWITCH_H

#include <wlr/types/wlr_switch.h>

struct wsm_seat;
struct wsm_seat_device;

/**
 * @brief Structure representing a switch in the WSM
 */
struct wsm_switch {
	struct wl_listener switch_toggle; /**< Listener for switch toggle events */

	struct wsm_seat_device *seat_device; /**< Pointer to the associated WSM seat device */
	struct wlr_switch *switch_wlr; /**< Pointer to the WLR switch instance */
	enum wlr_switch_state state; /**< Current state of the switch */
	enum wlr_switch_type type; /**< Type of the switch */
};

/**
 * @brief Creates a new wsm_switch instance
 * @param seat Pointer to the WSM seat associated with the switch
 * @param device Pointer to the WSM seat device associated with the switch
 * @return Pointer to the newly created wsm_switch instance
 */
struct wsm_switch *wsm_switch_create(struct wsm_seat *seat,
	struct wsm_seat_device *device);

/**
 * @brief Configures the specified wsm_switch instance
 * @param wsm_switch Pointer to the wsm_switch instance to configure
 */
void wsm_switch_configure(struct wsm_switch *wsm_switch);

/**
 * @brief Destroys the specified wsm_switch instance
 * @param wsm_switch Pointer to the wsm_switch instance to destroy
 */
void wsm_switch_destroy(struct wsm_switch *wsm_switch);

#endif
