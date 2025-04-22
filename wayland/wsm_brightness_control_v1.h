#ifndef WSM_BRIGHTNESS_CONTROL_V1_H
#define WSM_BRIGHTNESS_CONTROL_V1_H

#include <wayland-server-core.h>
#include <wayland-util.h>

struct wsm_output;

struct wsm_brightness_control_manager_v1 {
	struct wl_global *global;
	struct wl_list controls;

	struct {
		struct wl_signal destroy;
		struct wl_signal set_brightness;
	} events;

	void *data;
	struct wl_listener display_destroy;    
};

struct wsm_brightness_control_v1 {
	struct wl_resource *resource;
	struct wsm_output *output;
	struct wsm_brightness_control_manager_v1 *manager;
	struct wl_list link;

	long brightness;

	void *data;
	struct wl_listener output_destroy_listener;
};

struct wsm_brightness_control_manager_v1_set_brightness_event {
	struct wsm_output *output;
	struct wsm_brightness_control_v1 *control;
};

struct wsm_brightness_control_manager_v1 *wsm_brightness_control_manager_v1_create(
	struct wl_display *display);
struct wsm_brightness_control_v1 *wsm_brightness_control_manager_v1_get_output_brightness(
	struct wsm_brightness_control_manager_v1 *manager, struct wsm_output *output);
bool wsm_brightness_control_v1_set_brightness(struct wsm_brightness_control_v1 *brightness_control, long brightness);
void wsm_brightness_control_v1_failed_and_destroy(struct wsm_brightness_control_v1 *brightness_control);

#endif