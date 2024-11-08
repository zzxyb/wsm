#ifndef WSM_POINTER_H
#define WSM_POINTER_H

struct wlr_pointer;

struct wsm_seat;
struct wsm_seat_device;

/**
 * @brief Enumeration of pointer types in the WSM
 */
enum wsm_pointer_type {
	WSM_POINTER_TYPE_UNKNOWN, /**< Unknown pointer type */
	WSM_POINTER_TYPE_MOUSE, /**< Mouse pointer type */
	WSM_POINTER_TYPE_TOUCHPAD, /**< Touchpad pointer type */
	WSM_POINTER_TYPE_TRACKBALL, /**< Trackball pointer type */
	WSM_POINTER_TYPE_JOYSTICK, /**< Joystick pointer type */
	WSM_POINTER_TYPE_POINTING_STICK, /**< Pointing stick pointer type */
};

/**
 * @brief Structure representing a pointer in the WSM
 */
struct wsm_pointer {
	struct wlr_pointer *pointer_wlr; /**< Pointer to the associated WLR pointer instance */
	struct wsm_seat_device *seat_device_wsm; /**< Pointer to the associated WSM seat device */

	enum wsm_pointer_type pointer_type; /**< Type of the pointer */
};

/**
 * @brief Creates a new wsm_pointer instance
 * @param seat Pointer to the WSM seat associated with the pointer
 * @param device Pointer to the WSM seat device associated with the pointer
 * @return Pointer to the newly created wsm_pointer instance
 */
struct wsm_pointer *wsm_pointer_create(struct wsm_seat *seat,
	struct wsm_seat_device *device);

/**
 * @brief Destroys the specified wsm_pointer instance
 * @param pointer Pointer to the wsm_pointer instance to be destroyed
 */
void wsm_pointer_destroy(struct wsm_pointer *pointer);

#endif
