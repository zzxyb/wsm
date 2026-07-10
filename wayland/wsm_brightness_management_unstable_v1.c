#include "wsm_brightness_management_unstable_v1.h"

#include "wsm-brightness-management-unstable-v1-protocol.h"
#include "wsm_brightness.h"
#include "wsm_log.h"
#include "wsm_output.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include <wayland-server-core.h>
#include <wayland-server.h>
#include <wayland-util.h>
#include <wlr/types/wlr_output.h>

#define BRIGHTNESS_CONTROL_MANAGER_V1_VERSION 1

static const struct wsm_brightness_control_v1_interface brightness_control_impl;

static struct wsm_brightness_control_v1 *brightness_control_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource,
		&wsm_brightness_control_v1_interface, &brightness_control_impl));
	return wl_resource_get_user_data(resource);
}

static void brightness_control_destroy(
		struct wsm_brightness_control_v1 *control) {
	if (!control) {
		return;
	}
	wl_resource_set_user_data(control->resource, NULL);
	wl_list_remove(&control->output_destroy_listener.link);
	wl_list_remove(&control->link);
	free(control);
}

static void brightness_control_handle_resource_destroy(
		struct wl_resource *resource) {
	brightness_control_destroy(brightness_control_from_resource(resource));
}

static void brightness_control_handle_set_brightness(struct wl_client *client,
		struct wl_resource *resource, uint32_t value) {
	struct wsm_brightness_control_v1 *control =
		brightness_control_from_resource(resource);
	if (!control || !wsm_brightness_control_v1_set_brightness(control, value)) {
		wsm_brightness_control_v1_send_failed(resource);
	}
}

static void brightness_control_handle_destroy(struct wl_client *client,
		struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static const struct wsm_brightness_control_v1_interface
		brightness_control_impl = {
	.set_brightness = brightness_control_handle_set_brightness,
	.destroy = brightness_control_handle_destroy,
};

static const struct wsm_brightness_control_manager_v1_interface
	brightness_control_manager_impl;

static struct wsm_brightness_control_manager_v1 *
		brightness_control_manager_from_resource(struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource,
		&wsm_brightness_control_manager_v1_interface,
		&brightness_control_manager_impl));
	return wl_resource_get_user_data(resource);
}

static void brightness_control_handle_output_destroy(struct wl_listener *listener,
		void *data) {
	struct wsm_brightness_control_v1 *control =
		wl_container_of(listener, control, output_destroy_listener);
	wsm_brightness_control_v1_send_failed(control->resource);
	wl_resource_destroy(control->resource);
}

static uint32_t protocol_method(const struct wsm_brightness *brightness) {
	if (!brightness) {
		return WSM_BRIGHTNESS_CONTROL_V1_METHOD_NONE;
	}
	switch (brightness->method) {
	case WSM_BRIGHTNESS_METHOD_BACKLIGHT:
		return WSM_BRIGHTNESS_CONTROL_V1_METHOD_BACKLIGHT;
	case WSM_BRIGHTNESS_METHOD_DDCUTIL:
		return WSM_BRIGHTNESS_CONTROL_V1_METHOD_DDCUTIL;
	case WSM_BRIGHTNESS_METHOD_NONE:
		return WSM_BRIGHTNESS_CONTROL_V1_METHOD_NONE;
	}
	return WSM_BRIGHTNESS_CONTROL_V1_METHOD_NONE;
}

static bool brightness_values_valid(const struct wsm_brightness *brightness) {
	return brightness && brightness->brightness >= 0 &&
		brightness->min_brightness >= 0 && brightness->max_brightness >= 0 &&
		brightness->brightness <= UINT32_MAX &&
		brightness->min_brightness <= UINT32_MAX &&
		brightness->max_brightness <= UINT32_MAX;
}

static void send_initial_state(struct wl_resource *resource,
		const struct wsm_brightness *brightness) {
	uint32_t current = 0;
	uint32_t minimum = 0;
	uint32_t maximum = 0;
	if (brightness_values_valid(brightness)) {
		current = brightness->brightness;
		minimum = brightness->min_brightness;
		maximum = brightness->max_brightness;
	}
	wsm_brightness_control_v1_send_brightness(resource, current);
	wsm_brightness_control_v1_send_min_brightness(resource, minimum);
	wsm_brightness_control_v1_send_max_brightness(resource, maximum);
	wsm_brightness_control_v1_send_method(resource,
		protocol_method(brightness));
	wsm_brightness_control_v1_send_done(resource);
}

static void brightness_control_manager_get_output_brightness(
		struct wl_client *client, struct wl_resource *manager_resource,
		uint32_t id, struct wl_resource *output_resource) {
	struct wsm_brightness_control_manager_v1 *manager =
		brightness_control_manager_from_resource(manager_resource);
	uint32_t version = wl_resource_get_version(manager_resource);
	struct wl_resource *resource = wl_resource_create(client,
		&wsm_brightness_control_v1_interface, version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &brightness_control_impl,
		NULL, brightness_control_handle_resource_destroy);

	struct wlr_output *wlr_output = wlr_output_from_resource(output_resource);
	if (!wlr_output || !wlr_output->data) {
		wsm_brightness_control_v1_send_failed(resource);
		return;
	}
	struct wsm_output *output = wlr_output->data;
	if (wsm_brightness_control_manager_v1_get_output_brightness(
			manager, output)) {
		wsm_brightness_control_v1_send_failed(resource);
		return;
	}

	struct wsm_brightness_control_v1 *control = calloc(1, sizeof(*control));
	if (!control) {
		wl_client_post_no_memory(client);
		wl_resource_destroy(resource);
		return;
	}
	control->output = output;
	control->manager = manager;
	control->resource = resource;
	wl_resource_set_user_data(resource, control);
	control->output_destroy_listener.notify =
		brightness_control_handle_output_destroy;
	wl_signal_add(&wlr_output->events.destroy,
		&control->output_destroy_listener);
	wl_list_insert(&manager->controls, &control->link);
	send_initial_state(resource, output->brightness);
}

static void brightness_control_manager_destroy(struct wl_client *client,
		struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static const struct wsm_brightness_control_manager_v1_interface
		brightness_control_manager_impl = {
	.get_output_brightness = brightness_control_manager_get_output_brightness,
	.destroy = brightness_control_manager_destroy,
};

static void brightness_control_manager_client_bind(struct wl_client *client,
		void *data, uint32_t version, uint32_t id) {
	struct wsm_brightness_control_manager_v1 *manager = data;
	struct wl_resource *resource = wl_resource_create(client,
		&wsm_brightness_control_manager_v1_interface, version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource,
		&brightness_control_manager_impl, manager, NULL);
}

static void handle_display_destroy(struct wl_listener *listener, void *data) {
	struct wsm_brightness_control_manager_v1 *manager =
		wl_container_of(listener, manager, display_destroy);
	wl_signal_emit_mutable(&manager->events.destroy, manager);
	assert(wl_list_empty(&manager->events.destroy.listener_list));
	assert(wl_list_empty(&manager->events.set_brightness.listener_list));
	wl_list_remove(&manager->display_destroy.link);
	wl_global_destroy(manager->global);
	free(manager);
}

struct wsm_brightness_control_manager_v1 *
		wsm_brightness_control_manager_v1_create(struct wl_display *display) {
	struct wsm_brightness_control_manager_v1 *manager =
		calloc(1, sizeof(*manager));
	if (!manager) {
		wsm_log(WSM_ERROR, "Could not allocate brightness control manager");
		return NULL;
	}
	manager->global = wl_global_create(display,
		&wsm_brightness_control_manager_v1_interface,
		BRIGHTNESS_CONTROL_MANAGER_V1_VERSION, manager,
		brightness_control_manager_client_bind);
	if (!manager->global) {
		free(manager);
		return NULL;
	}
	wl_signal_init(&manager->events.destroy);
	wl_signal_init(&manager->events.set_brightness);
	wl_list_init(&manager->controls);
	manager->display_destroy.notify = handle_display_destroy;
	wl_display_add_destroy_listener(display, &manager->display_destroy);
	return manager;
}

struct wsm_brightness_control_v1 *
		wsm_brightness_control_manager_v1_get_output_brightness(
		struct wsm_brightness_control_manager_v1 *manager,
		struct wsm_output *output) {
	struct wsm_brightness_control_v1 *control;
	wl_list_for_each(control, &manager->controls, link) {
		if (control->output == output) {
			return control;
		}
	}
	return NULL;
}

bool wsm_brightness_control_v1_set_brightness(
		struct wsm_brightness_control_v1 *control, long brightness) {
	if (!control || !control->output || !control->output->brightness ||
			!wsm_brightness_set(control->output->brightness, brightness)) {
		return false;
	}
	struct wsm_brightness_control_manager_v1_set_brightness_event event = {
		.output = control->output,
		.control = control,
	};
	wl_signal_emit_mutable(&control->manager->events.set_brightness, &event);
	return true;
}

void wsm_brightness_control_v1_failed_and_destroy(
		struct wsm_brightness_control_v1 *control) {
	assert(control);
	wsm_brightness_control_v1_send_failed(control->resource);
	wl_resource_destroy(control->resource);
}
