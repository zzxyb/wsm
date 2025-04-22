#ifndef WSM_BACKLIGHT_DEVICE_H
#define WSM_BACKLIGHT_DEVICE_H

#include <stdbool.h>

struct wsm_output;

/**
 * @brief Enumeration of backlight device types
 *
 * @details Identifies and classifies different backlight control devices
 * so that they can be processed and optimized accordingly. Reference type in
 * the /sys/class/backlight directory.
 */
enum wsm_backlight_device_type {
	/**< Default value indicating an unknown backlight type. */
	WSM_BACKLIGHT_UNKNOW,
	/**< Low-level interface for direct control of backlight hardware. */
	WSM_BACKLIGHT_RAM,
	/**< Backlight control interface related to specific hardware
	 * platforms that may provide higher-level control through abstraction layers. */
	WSM_BACKLIGHT_PLATFORM,
	/**< Backlighting controlled by system firmware (such as BIOS or UEFI). */
	WSM_BACKLIGHT_FIRMWARE,
	/**< Specific backlight control devices, usually provided
	 * by the hardware manufacturer, which may include customized control mechanisms. */
	WSM_BACKLIGHT_VENDOR,
};

/**
 * @brief Structure representing a backlight device
 */
struct wsm_backlight_device {
	char *path;
	char *backlight_class; /**< Path to the backlight device */
	struct wsm_output* output; /**< Pointer to the associated output */
	struct wlr_session *session; /**< Pointer to the session */
	int max_brightness; /**< Maximum brightness level of the backlight */
	int brightness; /**< Current brightness level of the backlight */
	enum wsm_backlight_device_type type; /**< Type of the backlight device */
};

/**
 * @brief Creates a new wsm_backlight_device instance
 * @param output Pointer to the wsm_output associated with the backlight
 * @return Pointer to the newly created wsm_backlight_device instance
 */
struct wsm_backlight_device *wsm_backlight_device_create(struct wsm_output *output);

/**
 * @brief Destroys the specified wsm_backlight_device instance
 * @param backlight Pointer to the wsm_backlight_device instance to be destroyed
 */
void wsm_backlight_device_destroy(struct wsm_backlight_device *device);

/**
 * @brief Retrieves the maximum brightness level of the specified backlight
 * @param backlight Pointer to the wsm_backlight_device instance
 * @return Maximum brightness level
 */
long wsm_backlight_device_get_max_brightness(const struct wsm_backlight_device *device);

/**
 * @brief Retrieves the current brightness level of the specified backlight
 * @param backlight Pointer to the wsm_backlight_device instance
 * @return Current brightness level
 */
long wsm_backlight_device_get_brightness(const struct wsm_backlight_device *device);

/**
 * @brief Retrieves the actual brightness level of the specified backlight
 * @param backlight Pointer to the wsm_backlight_device instance
 * @return Actual brightness level
 */
long wsm_backlight_device_get_actual_brightness(const struct wsm_backlight_device *device);

/**
 * @brief Sets the brightness level of the specified backlight
 * @param backlight Pointer to the wsm_backlight_device instance
 * @param brightness New brightness level to set
 * @return true if the brightness was set successfully, false otherwise
 */
bool wsm_backlight_device_set_brightness(struct wsm_backlight_device *device, long brightness);

#endif
