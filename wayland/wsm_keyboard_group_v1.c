#include "wsm_keyboard_group_v1.h"

#include "wsm-keyboard-group-unstablev-1-protocol.h"
#include "wsm_input.h"
#include "wsm_input_manager.h"
#include "wsm_input_memory.h"
#include "wsm_keyboard.h"
#include "wsm_log.h"
#include "wsm_seat.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <wayland-server-protocol.h>

#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_keyboard_group.h>
#include <wlr/types/wlr_seat.h>

#include <xkbcommon/xkbcommon.h>

#define WSM_KEYBOARD_GROUP_MANAGER_V1_VERSION 1

static const struct wsm_keyboard_group_v1_interface keyboard_group_impl;
static const struct wsm_keyboard_group_manager_v1_interface manager_impl;

static struct wsm_keyboard_group_control_v1 *keyboard_group_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource,
		&wsm_keyboard_group_v1_interface, &keyboard_group_impl));
	return wl_resource_get_user_data(resource);
}

static struct wsm_keyboard_group_manager_v1 *manager_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource,
		&wsm_keyboard_group_manager_v1_interface, &manager_impl));
	return wl_resource_get_user_data(resource);
}

static void send_repeat_info(
		struct wsm_keyboard_group_manager_v1 *manager, int rate, int delay) {
	struct wsm_keyboard_group_control_v1 *group;
	wl_list_for_each(group, &manager->groups, link) {
		wsm_keyboard_group_v1_send_repeat_info(
			group->resource, rate, delay);
	}
}

static void send_start_num_lock(
		struct wsm_keyboard_group_manager_v1 *manager, bool enabled) {
	struct wsm_keyboard_group_control_v1 *group;
	wl_list_for_each(group, &manager->groups, link) {
		wsm_keyboard_group_v1_send_start_num_lock(
			group->resource, enabled);
	}
}

static void set_seat_repeat_info(struct wsm_seat *seat, int rate, int delay) {
	struct wsm_seat_device *device;
	wl_list_for_each(device, &seat->devices, link) {
		if (device->keyboard == NULL) {
			continue;
		}
		device->keyboard->repeat_rate = rate;
		device->keyboard->repeat_delay = delay;
		wlr_keyboard_set_repeat_info(device->keyboard->keyboard_wlr,
			rate, delay);
	}

	struct wsm_keyboard_group *group;
	wl_list_for_each(group, &seat->keyboard_groups, link) {
		wlr_keyboard_set_repeat_info(
			&group->keyboard_group_wlr->keyboard, rate, delay);
	}
}

static void set_keyboard_num_lock(
		struct wlr_keyboard *keyboard, bool enabled) {
	if (keyboard->keymap == NULL) {
		return;
	}
	xkb_mod_index_t index =
		xkb_keymap_mod_get_index(keyboard->keymap, XKB_MOD_NAME_NUM);
	if (index == XKB_MOD_INVALID || index >= sizeof(xkb_mod_mask_t) * 8) {
		return;
	}
	xkb_mod_mask_t mask = (xkb_mod_mask_t)1 << index;
	xkb_mod_mask_t locked = keyboard->modifiers.locked;
	if (enabled) {
		locked |= mask;
	} else {
		locked &= ~mask;
	}
	wlr_keyboard_notify_modifiers(keyboard,
		keyboard->modifiers.depressed, keyboard->modifiers.latched,
		locked, keyboard->modifiers.group);
}

static void set_seat_num_lock(struct wsm_seat *seat, bool enabled) {
	struct wsm_seat_device *device;
	wl_list_for_each(device, &seat->devices, link) {
		if (device->keyboard != NULL) {
			set_keyboard_num_lock(device->keyboard->keyboard_wlr, enabled);
		}
	}
}

static void handle_set_repeat_info(struct wl_client *client,
		struct wl_resource *resource, int32_t rate, int32_t delay) {
	struct wsm_keyboard_group_control_v1 *group =
		keyboard_group_from_resource(resource);
	if (rate < 0 || rate > 1000 || delay < 0 || delay > 10000) {
		wl_resource_post_error(resource,
			WSM_KEYBOARD_GROUP_V1_ERROR_INVALID_REPEAT_INFO,
			"invalid repeat info: rate=%d, delay=%d", rate, delay);
		return;
	}
	if (!wsm_input_memory_set_repeat_info(rate, delay)) {
		wl_resource_post_error(resource,
			WSM_KEYBOARD_GROUP_V1_ERROR_SAVE_FAILED,
			"failed to persist repeat info");
		return;
	}
	set_seat_repeat_info(group->seat, rate, delay);
	send_repeat_info(group->manager, rate, delay);
}

static void handle_set_start_num_lock(struct wl_client *client,
		struct wl_resource *resource, uint32_t enabled) {
	struct wsm_keyboard_group_control_v1 *group =
		keyboard_group_from_resource(resource);
	if (enabled > 1) {
		wl_resource_post_error(resource,
			WSM_KEYBOARD_GROUP_V1_ERROR_INVALID_NUM_LOCK,
			"invalid start_num_lock value: %u", enabled);
		return;
	}
	if (!wsm_input_memory_set_numlock(enabled)) {
		wl_resource_post_error(resource,
			WSM_KEYBOARD_GROUP_V1_ERROR_SAVE_FAILED,
			"failed to persist start_num_lock");
		return;
	}
	set_seat_num_lock(group->seat, enabled);
	send_start_num_lock(group->manager, enabled);
}

static void handle_group_destroy(struct wl_client *client,
		struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static void destroy_group(struct wsm_keyboard_group_control_v1 *group) {
	wl_resource_set_user_data(group->resource, NULL);
	wl_list_remove(&group->seat_destroy.link);
	wl_list_remove(&group->link);
	free(group);
}

static void handle_group_resource_destroy(struct wl_resource *resource) {
	struct wsm_keyboard_group_control_v1 *group =
		keyboard_group_from_resource(resource);
	if (group != NULL) {
		destroy_group(group);
	}
}

static const struct wsm_keyboard_group_v1_interface keyboard_group_impl = {
	.set_repeat_info = handle_set_repeat_info,
	.set_start_num_lock = handle_set_start_num_lock,
	.destroy = handle_group_destroy,
};

static void handle_seat_destroy(struct wl_listener *listener, void *data) {
	struct wsm_keyboard_group_control_v1 *group =
		wl_container_of(listener, group, seat_destroy);
	wl_resource_destroy(group->resource);
}

static struct wlr_seat_client *seat_client_from_keyboard_resource(
		struct wl_resource *resource) {
	if (resource == NULL ||
			strcmp(wl_resource_get_class(resource), "wl_keyboard") != 0) {
		return NULL;
	}
	struct wlr_seat_client *seat_client = wl_resource_get_user_data(resource);
	if (seat_client == NULL) {
		return NULL;
	}
	struct wl_resource *keyboard_resource;
	wl_resource_for_each(keyboard_resource, &seat_client->keyboards) {
		if (keyboard_resource == resource) {
			return seat_client;
		}
	}
	return NULL;
}

static void handle_get_keyboard_group(struct wl_client *client,
		struct wl_resource *manager_resource, uint32_t id,
		struct wl_resource *keyboard_resource) {
	struct wsm_keyboard_group_manager_v1 *manager =
		manager_from_resource(manager_resource);
	struct wlr_seat_client *seat_client =
		seat_client_from_keyboard_resource(keyboard_resource);
	if (seat_client == NULL) {
		wl_resource_post_error(manager_resource,
			WSM_KEYBOARD_GROUP_MANAGER_V1_ERROR_INVALID_KEYBOARD,
			"wl_keyboard is inert or invalid");
		return;
	}
	struct wsm_seat *seat =
		input_manager_seat_from_wlr_seat(seat_client->seat);
	if (seat == NULL) {
		wl_resource_post_error(manager_resource,
			WSM_KEYBOARD_GROUP_MANAGER_V1_ERROR_INVALID_KEYBOARD,
			"wl_keyboard has no WSM seat");
		return;
	}

	uint32_t version = wl_resource_get_version(manager_resource);
	struct wl_resource *resource = wl_resource_create(client,
		&wsm_keyboard_group_v1_interface, version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}
	struct wsm_keyboard_group_control_v1 *group = calloc(1, sizeof(*group));
	if (group == NULL) {
		wl_resource_destroy(resource);
		wl_client_post_no_memory(client);
		return;
	}
	group->resource = resource;
	group->manager = manager;
	group->seat = seat;
	wl_resource_set_implementation(resource, &keyboard_group_impl,
		group, handle_group_resource_destroy);
	wl_list_insert(&manager->groups, &group->link);
	group->seat_destroy.notify = handle_seat_destroy;
	wl_signal_add(&seat->seat->events.destroy, &group->seat_destroy);

	wsm_keyboard_group_v1_send_repeat_info(resource,
		wsm_input_memory_get_repeat_rate(),
		wsm_input_memory_get_repeat_delay());
	wsm_keyboard_group_v1_send_start_num_lock(resource,
		wsm_input_memory_get_numlock());
}

static void handle_manager_destroy(struct wl_client *client,
		struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static const struct wsm_keyboard_group_manager_v1_interface manager_impl = {
	.get_keyboard_group = handle_get_keyboard_group,
	.destroy = handle_manager_destroy,
};

static void handle_bind(struct wl_client *client, void *data,
		uint32_t version, uint32_t id) {
	struct wsm_keyboard_group_manager_v1 *manager = data;
	struct wl_resource *resource = wl_resource_create(client,
		&wsm_keyboard_group_manager_v1_interface, version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &manager_impl, manager, NULL);
}

static void handle_display_destroy(struct wl_listener *listener, void *data) {
	struct wsm_keyboard_group_manager_v1 *manager =
		wl_container_of(listener, manager, display_destroy);
	wl_list_remove(&manager->display_destroy.link);
	wl_global_destroy(manager->global);
	free(manager);
}

struct wsm_keyboard_group_manager_v1 *
wsm_keyboard_group_manager_v1_create(struct wl_display *display) {
	struct wsm_keyboard_group_manager_v1 *manager =
		calloc(1, sizeof(*manager));
	if (manager == NULL) {
		return NULL;
	}
	manager->global = wl_global_create(display,
		&wsm_keyboard_group_manager_v1_interface,
		WSM_KEYBOARD_GROUP_MANAGER_V1_VERSION, manager, handle_bind);
	if (manager->global == NULL) {
		free(manager);
		return NULL;
	}
	wl_list_init(&manager->groups);
	manager->display_destroy.notify = handle_display_destroy;
	wl_display_add_destroy_listener(display, &manager->display_destroy);
	return manager;
}
