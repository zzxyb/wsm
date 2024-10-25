#ifndef WSM_INPUT_H
#define WSM_INPUT_H

#include <stdbool.h>

#include <libinput.h>

#include <wayland-server-core.h>

struct libinput_device;

struct wlr_input_device;

struct wsm_input_device_impl;

struct wsm_input_device {
	struct wl_listener device_destroy;
	struct wl_list link;

	char *identifier;
	struct wlr_input_device *input_device_wlr;
	const struct wsm_input_device_impl *input_device_impl_wsm;
	bool is_virtual;
};

struct wsm_input_device_impl {
	enum libinput_config_status (*set_send_events)
		(struct libinput_device *device, uint32_t mode);
	enum libinput_config_status (*set_tap)
		(struct libinput_device *device, enum libinput_config_tap_state tap);
	enum libinput_config_status (*set_tap_button_map)
		(struct libinput_device *device, enum libinput_config_tap_button_map map);
	enum libinput_config_status (*set_tap_drag)
		(struct libinput_device *device, enum libinput_config_drag_state drag);
	enum libinput_config_status (*set_tap_drag_lock)
		(struct libinput_device *device, enum libinput_config_drag_lock_state lock);
	enum libinput_config_status (*set_accel_speed)(struct libinput_device *device, double speed);
	enum libinput_config_status (*set_rotation_angle)(struct libinput_device *device, double angle);
	enum libinput_config_status (*set_accel_profile)
		(struct libinput_device *device, enum libinput_config_accel_profile profile);
	enum libinput_config_status (*set_natural_scroll)(struct libinput_device *d, bool n);
	enum libinput_config_status (*set_left_handed)(struct libinput_device *device, bool left);
	enum libinput_config_status (*set_click_method)
		(struct libinput_device *device, enum libinput_config_click_method method);
	enum libinput_config_status (*set_middle_emulation)
		(struct libinput_device *dev, enum libinput_config_middle_emulation_state mid);
	enum libinput_config_status (*set_scroll_method)
		(struct libinput_device *device, enum libinput_config_scroll_method method);
	enum libinput_config_status (*set_scroll_button)(struct libinput_device *dev, uint32_t button);
	enum libinput_config_status (*set_scroll_button_lock)
		(struct libinput_device *dev, enum libinput_config_scroll_button_lock_state lock);
	enum libinput_config_status (*set_dwt)(struct libinput_device *device, bool dwt);
	enum libinput_config_status (*set_dwtp)(struct libinput_device *device, bool dwtp);
	enum libinput_config_status (*set_calibration_matrix)(struct libinput_device *dev, float mat[6]);
};

struct wsm_input_device* wsm_input_device_create();
struct wsm_input_device *input_wsm_device_from_wlr(struct wlr_input_device *device);
void wsm_input_device_destroy(struct wlr_input_device *wlr_device);
void wsm_input_configure_libinput_device_send_events(struct wsm_input_device *input_device);

#endif
