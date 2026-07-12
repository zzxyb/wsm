#ifndef WSM_KEYBOARD_GROUP_V1_H
#define WSM_KEYBOARD_GROUP_V1_H

#include <stdbool.h>
#include <stdint.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

struct wsm_seat;

struct wsm_keyboard_group_manager_v1 {
	struct wl_global *global;
	struct wl_list groups;
	struct wl_listener display_destroy;
};

struct wsm_keyboard_group_control_v1 {
	struct wl_resource *resource;
	struct wsm_keyboard_group_manager_v1 *manager;
	struct wsm_seat *seat;
	struct wl_list link;
	struct wl_listener seat_destroy;
};

struct wsm_keyboard_group_manager_v1 *
wsm_keyboard_group_manager_v1_create(struct wl_display *display);

#endif
