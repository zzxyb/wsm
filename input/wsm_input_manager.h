#ifndef WSM_INPUT_MANAGER_H
#define WSM_INPUT_MANAGER_H

#include <stdbool.h>

#include <libinput.h>

#include <wlr/types/wlr_keyboard_shortcuts_inhibit_v1.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlr/types/wlr_virtual_pointer_v1.h>

struct wlr_seat;
struct wlr_pointer_gestures_v1;

struct wsm_seat;
struct wsm_node;
struct wsm_server;
struct input_config;
struct wsm_input_device;

struct wsm_input_manager {
	struct wl_listener new_input;
	struct wl_listener inhibit_activate;
	struct wl_listener inhibit_deactivate;
	struct wl_listener keyboard_shortcuts_inhibit_new_inhibitor;
	struct wl_listener virtual_keyboard_new;
	struct wl_listener virtual_pointer_new;

	struct wl_list devices;
	struct wl_list seats;

	struct wlr_keyboard_shortcuts_inhibit_manager_v1 *keyboard_shortcuts_inhibit;
	struct wlr_virtual_keyboard_manager_v1 *virtual_keyboard;
	struct wlr_virtual_pointer_manager_v1 *virtual_pointer;
	struct wlr_pointer_gestures_v1 *pointer_gestures;
};

struct wsm_input_manager *wsm_input_manager_create(const struct wsm_server* server);
struct wsm_seat *input_manager_get_default_seat();
struct wsm_seat *input_manager_current_seat(void);
struct wsm_seat *input_manager_get_seat(const char *seat_name, bool create);
struct wsm_seat *input_manager_seat_from_wlr_seat(struct wlr_seat *wlr_seat);
char *input_device_get_identifier(struct wlr_input_device *device);
void input_manager_configure_xcursor(void);
void input_manager_set_focus(struct wsm_node *node);
void input_manager_configure_all_input_mappings(void);
struct input_config *input_device_get_config(struct wsm_input_device *device);
const char *input_device_get_type(struct wsm_input_device *device);

#endif
